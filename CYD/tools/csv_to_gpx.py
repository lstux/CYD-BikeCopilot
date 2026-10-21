#!/usr/bin/env python3
"""Convertit un trajet enregistré par le CYD-BikeCopilot (CSV) en GPX (Strava, Komoot, etc.).

Usage : python3 tools/csv_to_gpx.py trips/20261018_160023.csv [sortie.gpx]
"""
import csv
import sys
from datetime import datetime, timezone
from xml.sax.saxutils import escape


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src.rsplit(".", 1)[0] + ".gpx"
    pts = []
    with open(src, newline="") as f:
        for row in csv.DictReader(f):
            try:
                pts.append((int(float(row["t"])), float(row["lat"]), float(row["lon"]), float(row["alt"])))
            except (KeyError, ValueError):
                continue
    if len(pts) < 2:
        sys.exit("Pas assez de points")
    name = escape(src.replace("\\", "/").rsplit("/", 1)[-1].rsplit(".", 1)[0])
    with open(dst, "w", encoding="utf-8") as g:
        g.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        g.write('<gpx version="1.1" creator="CYD-BikeCopilot" xmlns="http://www.topografix.com/GPX/1/1">\n')
        g.write("<trk><name>%s</name><type>cycling</type><trkseg>\n" % name)
        for t, la, lo, alt in pts:
            iso = datetime.fromtimestamp(t, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
            g.write('<trkpt lat="%.6f" lon="%.6f"><ele>%.1f</ele><time>%s</time></trkpt>\n' % (la, lo, alt, iso))
        g.write("</trkseg></trk>\n</gpx>\n")
    print("%d points -> %s" % (len(pts), dst))


if __name__ == "__main__":
    main()
