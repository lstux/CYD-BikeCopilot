#!/usr/bin/env python3
"""Prépare les tuiles de carte pour la carte SD du CYD-BikeCopilot.

Sortie : <out>/tiles/<z>/<x>/<y>.raw  (256x256, RGB565 big-endian, 131072 octets)
         + copie optionnelle de la trace en <out>/route.gpx

Sources de tuiles :
  --png-dir DIR   convertit des tuiles PNG déjà présentes (DIR/<z>/<x>/<y>.png)
  --url TEMPLATE  télécharge (défaut : serveur OSM, {z}/{x}/{y}) — limité, voir ci-dessous

Politique d'usage des tuiles OSM : pas de téléchargement massif, User-Agent identifiant,
pas plus de 2 requêtes/s. Pour une grande zone, utilisez un fournisseur de tuiles adapté
ou --png-dir avec des tuiles obtenues légalement. Ce script plafonne à --max-tiles.

Exemples :
  python3 tools/make_tiles.py --gpx route.gpx --zoom 15 16 17 --out sd/
  python3 tools/make_tiles.py --bbox 48.84 2.28 48.87 2.33 --zoom 16 --out sd/
  python3 tools/make_tiles.py --png-dir mes_tuiles_png --zoom 16 --out sd/
"""
import argparse
import math
import os
import re
import shutil
import sys
import time
import urllib.request
from io import BytesIO

from PIL import Image

DEFAULT_URL = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"


def deg2tile(lat, lon, z):
    n = 2 ** z
    x = int((lon + 180.0) / 360.0 * n)
    s = math.sin(math.radians(lat))
    y = int((0.5 - math.log((1 + s) / (1 - s)) / (4 * math.pi)) * n)
    return max(0, min(n - 1, x)), max(0, min(n - 1, y))


def read_gpx(path):
    txt = open(path, encoding="utf-8", errors="replace").read()
    pts = []
    for tag in ("trkpt", "rtept"):
        for m in re.finditer(r"<%s\b([^>]*)>" % tag, txt):
            a = m.group(1)
            la = re.search(r'\blat\s*=\s*["\']([-0-9.eE+]+)', a)
            lo = re.search(r'\blon\s*=\s*["\']([-0-9.eE+]+)', a)
            if la and lo:
                pts.append((float(la.group(1)), float(lo.group(1))))
        if len(pts) >= 2:
            break
    if len(pts) < 2:
        sys.exit("GPX : moins de 2 points trouvés")
    return pts


def tiles_for_points(pts, z, margin):
    """Tuiles touchées par la trace, élargies de `margin` tuiles autour."""
    s = set()
    prev = None
    for lat, lon in pts:
        x, y = deg2tile(lat, lon, z)
        cells = [(x, y)]
        if prev:  # combler les trous entre deux points éloignés
            px, py = prev
            steps = max(abs(x - px), abs(y - py))
            for i in range(1, steps):
                cells.append((round(px + (x - px) * i / steps), round(py + (y - py) * i / steps)))
        prev = (x, y)
        for cx, cy in cells:
            for dx in range(-margin, margin + 1):
                for dy in range(-margin, margin + 1):
                    s.add((cx + dx, cy + dy))
    n = 2 ** z
    return sorted((x, y) for x, y in s if 0 <= x < n and 0 <= y < n)


def tiles_for_bbox(lat1, lon1, lat2, lon2, z):
    x1, y1 = deg2tile(max(lat1, lat2), min(lon1, lon2), z)
    x2, y2 = deg2tile(min(lat1, lat2), max(lon1, lon2), z)
    return [(x, y) for x in range(x1, x2 + 1) for y in range(y1, y2 + 1)]


def to_rgb565_be(img):
    img = img.convert("RGB")
    if img.size != (256, 256):
        img = img.resize((256, 256), Image.LANCZOS)
    out = bytearray(256 * 256 * 2)
    i = 0
    raw = img.tobytes()
    for k in range(0, len(raw), 3):
        r, g, b = raw[k], raw[k + 1], raw[k + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out[i] = v >> 8
        out[i + 1] = v & 0xFF
        i += 2
    return bytes(out)


def fetch(url, ua):
    req = urllib.request.Request(url, headers={"User-Agent": ua})
    with urllib.request.urlopen(req, timeout=20) as r:
        return r.read()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--gpx", help="trace GPX : tuiles le long du parcours")
    ap.add_argument("--bbox", nargs=4, type=float, metavar=("LAT1", "LON1", "LAT2", "LON2"))
    ap.add_argument("--zoom", nargs="+", type=int, default=[15, 16, 17])
    ap.add_argument("--margin", type=int, default=1, help="tuiles de marge autour de la trace (déf. 1)")
    ap.add_argument("--out", default="sd", help="dossier de sortie (à copier à la racine de la SD)")
    ap.add_argument("--png-dir", help="convertir des tuiles PNG locales (<z>/<x>/<y>.png)")
    ap.add_argument("--url", default=DEFAULT_URL, help="modèle d'URL avec {z} {x} {y}")
    ap.add_argument("--user-agent", default="CYD-BikeCopilot-tiles/1.0 (usage personnel)")
    ap.add_argument("--delay", type=float, default=0.6, help="secondes entre deux requêtes")
    ap.add_argument("--max-tiles", type=int, default=400, help="plafond de tuiles à télécharger")
    ap.add_argument("--dry-run", action="store_true", help="compter les tuiles sans rien faire")
    a = ap.parse_args()

    if not a.gpx and not a.bbox and not a.png_dir:
        ap.error("donnez --gpx, --bbox ou --png-dir")

    pts = read_gpx(a.gpx) if a.gpx else None
    todo = []
    for z in a.zoom:
        if pts:
            t = tiles_for_points(pts, z, a.margin)
        elif a.bbox:
            t = tiles_for_bbox(*a.bbox, z)
        else:  # png-dir : tout ce qui existe
            d = os.path.join(a.png_dir, str(z))
            t = []
            if os.path.isdir(d):
                for xs in os.listdir(d):
                    for f in os.listdir(os.path.join(d, xs)):
                        if f.endswith(".png") and xs.isdigit():
                            t.append((int(xs), int(f[:-4])))
        print("zoom %d : %d tuiles (%.1f Mo)" % (z, len(t), len(t) * 128 / 1024))
        todo += [(z, x, y) for x, y in t]

    print("total : %d tuiles, %.1f Mo" % (len(todo), len(todo) * 128 / 1024))
    if a.dry_run:
        return
    if not a.png_dir and len(todo) > a.max_tiles:
        sys.exit("Trop de tuiles pour un téléchargement depuis un serveur public (%d > %d).\n"
                 "Réduisez la zone/les zooms, utilisez un autre fournisseur, ou --max-tiles (avec modération)."
                 % (len(todo), a.max_tiles))

    done = skipped = failed = 0
    for i, (z, x, y) in enumerate(todo, 1):
        dst = os.path.join(a.out, "tiles", str(z), str(x), "%d.raw" % y)
        if os.path.exists(dst) and os.path.getsize(dst) == 131072:
            skipped += 1
            continue
        try:
            if a.png_dir:
                img = Image.open(os.path.join(a.png_dir, str(z), str(x), "%d.png" % y))
            else:
                data = fetch(a.url.format(z=z, x=x, y=y), a.user_agent)
                img = Image.open(BytesIO(data))
                time.sleep(a.delay)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, "wb") as f:
                f.write(to_rgb565_be(img))
            done += 1
        except Exception as e:  # noqa: BLE001
            failed += 1
            print("  échec %d/%d/%d : %s" % (z, x, y, e))
        if i % 25 == 0:
            print("  %d/%d" % (i, len(todo)))

    if a.gpx:
        os.makedirs(a.out, exist_ok=True)
        shutil.copyfile(a.gpx, os.path.join(a.out, "route.gpx"))
    print("fait : %d écrites, %d déjà présentes, %d échecs → copiez le contenu de '%s' à la racine de la SD"
          % (done, skipped, failed, a.out))


if __name__ == "__main__":
    main()
