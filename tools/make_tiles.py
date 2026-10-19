#!/usr/bin/env python3
"""Prépare les tuiles de carte pour la carte SD du CYD-BikeCopilot.
Dépendances : pip install pillow numpy

Sortie : <out>/tiles_<thème>/<z>/<x>/<y>.raw  (256x256, RGB565 big-endian, 131072 octets)
         un dossier par thème : tiles_dark, tiles_light (ou tiles_osm), lus par le firmware selon le
         thème de la carte (Réglages : Auto / Sombre / Clair)
         + copie optionnelle de la trace en <out>/route.gpx

Les tuiles ORIGINALES (PNG) sont conservées dans un cache :
  <cache>/<style>/<z>/<x>/<y>.png     (par défaut tiles_src/osm/...)
Une tuile n'est donc téléchargée qu'une seule fois ; on peut ensuite changer de thème ou de
traitement et reconvertir sans réseau (--offline), en écrasant les anciens .raw (--force).
Plusieurs thèmes en un seul passage : --theme dark light (les originales ne sont lues qu'une fois).

Sources de tuiles :
  (défaut)        cache puis téléchargement (serveur OSM, {z}/{x}/{y})
  --url TEMPLATE  autre fournisseur ; donnez aussi --style NOM pour un cache séparé
  --png-dir DIR   convertit des tuiles PNG déjà présentes (DIR/<z>/<x>/<y>.png)

Politique d'usage des tuiles OSM : pas de téléchargement massif, User-Agent identifiant,
pas plus de 2 requêtes/s. Pour une grande zone, utilisez un fournisseur de tuiles adapté
ou --png-dir avec des tuiles obtenues légalement. Ce script plafonne à --max-tiles le nombre
de tuiles À TÉLÉCHARGER (celles déjà dans le cache ne comptent pas).

Exemples :
  python3 tools/make_tiles.py --gpx route.gpx --zoom 15 16 17 --out sd/
  python3 tools/make_tiles.py --bbox 48.84 2.28 48.87 2.33 --zoom 16 --out sd/
  python3 tools/make_tiles.py --bbox 48.84 2.28 48.87 2.33 --zoom 16 --out sd/ --offline --force --theme dark light
  python3 tools/make_tiles.py --png-dir mes_tuiles_png --zoom 16 --out sd/

La page tools/tiles-manager.html aide à choisir les zones et génère ces commandes.
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

import numpy as np
from PIL import Image, ImageFilter

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


# ---------------------------------------------------------------------------------------------
# Recoloration des tuiles OSM (thèmes "dark" et "light"), pensée pour un petit écran TFT vu
# sous des angles variables : fort contraste route / fond, hiérarchie des routes par couleur,
# bâtiments / eau / parcs en retrait, texte lisible.
#
# Principe : une tuile OSM n'utilise qu'une trentaine de couleurs (+ leurs mélanges dus à
# l'anti-crénelage). Chaque pixel est décrit comme un mélange de deux couleurs connues et
# remplacé par le mélange des couleurs de sortie correspondantes. Le texte (noir avec halo blanc)
# est traité à part : halo effacé, encre claire sur fond sombre (ou sombre sur une route claire).
# Une couleur inconnue (pictogramme, style exotique) est simplement atténuée / inversée.
# --theme osm : aucune recoloration (couleurs d'origine).
# ---------------------------------------------------------------------------------------------
def _hx(s):
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))


# (couleur OSM d'origine, couleur de sortie)
_DARK = [
    ("f2efe9", "0b1017"), ("e0dfdf", "0e141c"), ("eef0d5", "0d151a"), ("ebdbe8", "141320"),   # sols
    ("f2dad9", "161320"), ("ffd6d1", "181320"), ("f0f0d8", "0f161b"), ("ffffe5", "0f161b"),
    ("cdebb0", "0f2a20"), ("c8facc", "10301f"), ("add19e", "0d3322"), ("c8d7ab", "112a1f"),   # herbe, parcs, bois
    ("aad3df", "0c2a55"),                                                                     # eau
    ("d9d0c9", "1b232d"), ("c2b5aa", "34424f"), ("d1c6bd", "27323d"),                         # bâtiments
    ("ffffff", "a5c4d8"), ("fefefe", "a5c4d8"), ("eeeeee", "5f7587"), ("bbbbbb", "5a7080"),   # rues
    ("f7fabe", "ffe58a"), ("fcd6a4", "ffbd5e"), ("f9b29c", "ff8a63"), ("e892a2", "ff6f98"),   # secondaires -> autoroutes
    ("dc2a67", "9a3a5a"), ("c84e2f", "98513a"), ("a06b00", "8f6a2a"), ("a7a574", "8a8650"),   # liserés
    ("8f8f8f", "4d6272"),
    ("fa8072", "b87c72"), ("0000ff", "5c9bff"), ("0092da", "3fb0f0"), ("808080", "6d8294"),   # chemins, pistes, pictos, rails
    ("000000", "ecf3f9"),                                                                     # texte
]
_LIGHT = [
    ("f2efe9", "ecedf0"), ("e0dfdf", "e0e3e8"), ("eef0d5", "e9ede6"), ("ebdbe8", "e4dfe8"),
    ("f2dad9", "eadfe0"), ("ffd6d1", "ecdcdc"), ("f0f0d8", "e8ebe3"), ("ffffe5", "e8ebe3"),
    ("cdebb0", "c4e0bd"), ("c8facc", "bfe8c4"), ("add19e", "a8cfa2"), ("c8d7ab", "c6d9b0"),
    ("aad3df", "8cc0ea"),
    ("d9d0c9", "bfc4cc"), ("c2b5aa", "8d96a3"), ("d1c6bd", "aab0ba"),
    ("ffffff", "27506f"), ("fefefe", "27506f"), ("eeeeee", "a4b0bc"), ("bbbbbb", "7d8c9a"),
    ("f7fabe", "d6a800"), ("fcd6a4", "ee8a12"), ("f9b29c", "dc4a2c"), ("e892a2", "c8205f"),
    ("dc2a67", "7a1238"), ("c84e2f", "7e2c1a"), ("a06b00", "6e4800"), ("a7a574", "6e6a30"),
    ("8f8f8f", "566472"),
    ("fa8072", "d0604f"), ("0000ff", "2450e0"), ("0092da", "0a78c0"), ("808080", "50606e"),
    ("000000", "0c121a"),
]
# fond, encre sur fond, encre sur route, repli pour couleur inconnue : L' = a + b*(100-L) (sombre) ou a + b*L (clair)
_THEMES = {
    "dark": dict(anchors=_DARK, bg=(11, 16, 23), ink_bg=(236, 243, 249), ink_road=(8, 14, 22), fb=(18, 0.38, True)),
    "light": dict(anchors=_LIGHT, bg=(236, 238, 240), ink_bg=(12, 18, 26), ink_road=(246, 249, 251), fb=(8, 0.9, False)),
}
_CACHE = {}

_M = np.array([[0.4124564, 0.3575761, 0.1804375], [0.2126729, 0.7151522, 0.0721750], [0.0193339, 0.1191920, 0.9503041]])
_WP = np.array([0.95047, 1.0, 1.08883])


def _rgb2lab(rgb):
    c = rgb / 255.0
    c = np.where(c > 0.04045, ((c + 0.055) / 1.055) ** 2.4, c / 12.92)
    xyz = (c @ _M.T) / _WP
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)


def _lab2rgb(lab):
    fy = (lab[..., 0] + 16) / 116
    f = np.stack([fy + lab[..., 1] / 500, fy, fy - lab[..., 2] / 200], -1)
    xyz = np.where(f ** 3 > 0.008856, f ** 3, (f - 16 / 116) / 7.787) * _WP
    c = xyz @ np.linalg.inv(_M).T
    c = np.where(c > 0.0031308, 1.055 * np.clip(c, 0, None) ** (1 / 2.4) - 0.055, 12.92 * c)
    return np.clip(c * 255 + 0.5, 0, 255)


def _boxf(a, k):
    """Moyenne glissante k x k (k impair), bords répliqués."""
    r = k // 2
    c = np.pad(np.pad(a.astype(float), r, mode="edge").cumsum(0).cumsum(1), ((1, 0), (1, 0)))
    h, w = a.shape
    return (c[k:k + h, k:k + w] - c[:h, k:k + w] - c[k:k + h, :w] + c[:h, :w]) / (k * k)


def _mf(mask, size, mode="max"):
    im = Image.fromarray((mask * 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.MaxFilter(size) if mode == "max" else ImageFilter.MinFilter(size))) > 127


def _recolor(src, th):
    """src : (h, w, 3) float. Chaque couleur est projetée sur le segment le plus proche entre deux
    couleurs d'origine connues, puis remplacée par le mélange des couleurs de sortie."""
    h, w, _ = src.shape
    uniq, inv = np.unique(src.reshape(-1, 3), axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    A = np.array([_hx(x) for x, _ in th["anchors"]], float)
    T = np.array([_hx(y) for _, y in th["anchors"]], float)
    best = np.full(len(uniq), 1e9)
    res = np.zeros(len(uniq))
    out = np.zeros_like(uniq)
    for i in range(len(A)):
        for j in range(i, len(A)):
            if i == j:
                t = np.zeros(len(uniq))
                q = np.broadcast_to(A[i], uniq.shape)
                pen = 0.0
            else:
                d = A[j] - A[i]
                l2 = float(d @ d)
                t = np.clip(((uniq - A[i]) @ d) / l2, 0, 1)
                q = A[i] + t[:, None] * d
                pen = 0.02 * np.sqrt(l2)
            r = np.sqrt(((uniq - q) ** 2).sum(1))
            m = (r + pen) < best
            if m.any():
                best[m] = (r + pen)[m]
                res[m] = r[m]
                out[m] = T[i] + t[m, None] * (T[j] - T[i])
    lab = _rgb2lab(uniq)
    off, slope, inv_l = th["fb"]
    lab[:, 0] = off + slope * (100 - lab[:, 0]) if inv_l else off + slope * lab[:, 0]
    lab[:, 1:] *= 0.55
    alpha = np.clip(1 - (res - 5.0) / 12.0, 0, 1)[:, None]
    return np.clip(alpha * out + (1 - alpha) * _lab2rgb(lab), 0, 255)[inv].reshape(h, w, 3)


def recolor_tile(img, z, theme):
    th = _THEMES[theme]
    src = np.asarray(img.convert("RGB"), float)
    out = _recolor(src, th)
    lum = src @ np.array([0.299, 0.587, 0.114])
    neutral = (src.max(-1) - src.min(-1)) < 24
    ink = neutral & (lum < 110)                         # texte
    white = neutral & (lum > 236)                       # route, halo...
    if z >= 16:
        core = _mf(_mf(white, 5, "min"), 5, "max")      # routes larges seulement (les halos font 1-2 px)
    elif z == 15:
        core = _mf(_mf(white & ~_mf(ink, 3), 3, "min"), 3, "max")
    else:
        core = np.zeros_like(white)
    zone = _mf(ink, 5)                                  # texte + halo
    if zone.any():
        keep = (~zone).astype(float)
        num = np.stack([_boxf(out[..., c] * keep, 9) for c in range(3)], -1)
        den = _boxf(keep, 9)[..., None]
        local = np.where(den > 0.04, num / np.maximum(den, 1e-3), np.array(th["bg"], float))
        base = np.where((zone & ~core)[..., None], local, out)       # halo effacé hors des routes
        on_road = (_boxf(core.astype(float), 9) > 0.25)[..., None]
        color = np.where(on_road, np.array(th["ink_road"], float), np.array(th["ink_bg"], float))
        a = np.clip((238 - lum) / 200.0, 0, 1)[..., None]            # 0 = halo / fond, 1 = encre
        out = np.where(zone[..., None], base * (1 - a) + color * a, out)
    return np.clip(out + 0.5, 0, 255).astype(np.uint8)


def to_rgb565_be(img, z, theme):
    img = img.convert("RGB")
    if img.size != (256, 256):
        img = img.resize((256, 256), Image.LANCZOS)
    a = np.asarray(img) if theme == "osm" else recolor_tile(img, z, theme)
    a = a.astype(np.uint16)
    v = ((a[..., 0] & 0xF8) << 8) | ((a[..., 1] & 0xFC) << 3) | (a[..., 2] >> 3)
    return v.astype(">u2").tobytes()


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
    ap.add_argument("--style", default="osm", help="nom du fond de carte = sous-dossier du cache (déf. osm)")
    ap.add_argument("--cache", default="tiles_src", help="dossier des tuiles originales PNG (déf. tiles_src)")
    ap.add_argument("--offline", action="store_true", help="ne rien télécharger : convertir seulement ce qui est dans le cache")
    ap.add_argument("--force", action="store_true", help="réécrire les .raw déjà présents (après un changement de thème)")
    ap.add_argument("--user-agent", default="CYD-BikeCopilot-tiles/1.0 (usage personnel)")
    ap.add_argument("--delay", type=float, default=0.6, help="secondes entre deux requêtes")
    ap.add_argument("--max-tiles", type=int, default=400, help="plafond de tuiles à télécharger")
    ap.add_argument("--theme", nargs="+", choices=("dark", "light", "osm"), default=["dark", "light"],
                    help="un ou plusieurs thèmes (déf. : dark light) : dark = fond sombre ; light = fond clair, "
                         "routes foncées (plein soleil) ; osm = couleurs d'origine")
    ap.add_argument("--dry-run", action="store_true", help="compter les tuiles sans rien faire")
    a = ap.parse_args()
    themes = list(dict.fromkeys(a.theme))

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

    def src_path(z, x, y):
        if a.png_dir:
            return os.path.join(a.png_dir, str(z), str(x), "%d.png" % y)
        return os.path.join(a.cache, a.style, str(z), str(x), "%d.png" % y)

    missing = [t for t in todo if not os.path.exists(src_path(*t))]
    print("total : %d tuiles x %d thème(s), %.1f Mo sur la SD ; %d dans le cache, %d à télécharger"
          % (len(todo), len(themes), len(todo) * len(themes) * 128 / 1024, len(todo) - len(missing), len(missing)))
    if a.dry_run:
        return
    if not a.png_dir and not a.offline and len(missing) > a.max_tiles:
        sys.exit("Trop de tuiles à télécharger depuis un serveur public (%d > %d).\n"
                 "Réduisez la zone/les zooms, utilisez un autre fournisseur, ou --max-tiles (avec modération)."
                 % (len(missing), a.max_tiles))

    done = skipped = failed = unavailable = 0
    for i, (z, x, y) in enumerate(todo, 1):
        dsts = {th: os.path.join(a.out, "tiles_" + th, str(z), str(x), "%d.raw" % y) for th in themes}
        need = [th for th in themes
                if a.force or not (os.path.exists(dsts[th]) and os.path.getsize(dsts[th]) == 131072)]
        skipped += len(themes) - len(need)
        if not need:
            continue
        src = src_path(z, x, y)
        try:
            if os.path.exists(src):
                img = Image.open(src)
                img.load()
            elif a.png_dir or a.offline:
                unavailable += 1
                continue
            else:
                data = fetch(a.url.format(z=z, x=x, y=y), a.user_agent)
                img = Image.open(BytesIO(data))
                img.load()
                os.makedirs(os.path.dirname(src), exist_ok=True)  # on garde l'original tel quel
                with open(src, "wb") as f:
                    f.write(data)
                time.sleep(a.delay)
            for th in need:
                os.makedirs(os.path.dirname(dsts[th]), exist_ok=True)
                with open(dsts[th], "wb") as f:
                    f.write(to_rgb565_be(img, z, th))
                done += 1
        except Exception as e:  # noqa: BLE001
            failed += 1
            print("  échec %d/%d/%d : %s" % (z, x, y, e))
        if i % 25 == 0:
            print("  %d/%d" % (i, len(todo)))

    if a.gpx:
        os.makedirs(a.out, exist_ok=True)
        shutil.copyfile(a.gpx, os.path.join(a.out, "route.gpx"))
    print("fait : %d écrites, %d déjà présentes, %d échecs%s → copiez le contenu de '%s' à la racine de la SD"
          % (done, skipped, failed, (", %d absentes du cache" % unavailable) if unavailable else "", a.out))


if __name__ == "__main__":
    main()
