#!/usr/bin/env python3
"""Génère src/dashboard/fonts_data.h : polices lissées (alpha 8 bits) extraites d'Inter.

Inter (SIL Open Font License 1.1, https://rsms.me/inter/) doit être installée : fichiers
Inter-Bold.otf, Inter-SemiBold.otf, Inter-Medium.otf (paquet Debian `fonts-inter`, ou
téléchargement sur le site). Usage :  python3 tools/make_fonts.py [dossier_des_polices]
Le fichier généré est versionné : il n'est à régénérer que pour changer polices ou tailles.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

FONT_DIR = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/opentype/inter"
OUT = os.path.join(os.path.dirname(__file__), "..", "src", "dashboard", "fonts_data.h")

DIG = "0123456789"
NUM = DIG + ".:-+°%/ h!−"
ACC = "éèêëàâîïôùûçÉÈÀÊ°·−–›‹…'’©"
TXT = "".join(chr(c) for c in range(32, 127)) + ACC

# (nom, fichier, taille px, caractères, chiffres à chasse fixe)
FONTS = [
    ("B56", "Inter-Bold.otf", 56, DIG, True),
    ("B30", "Inter-Bold.otf", 30, NUM, True),
    ("B21", "Inter-Bold.otf", 21, NUM, True),
    ("B17", "Inter-Bold.otf", 17, TXT, True),
    ("B13", "Inter-Bold.otf", 13, TXT, True),
    ("S13", "Inter-SemiBold.otf", 13, TXT, True),
    ("S11", "Inter-SemiBold.otf", 11, TXT, True),
    ("S9", "Inter-SemiBold.otf", 9, TXT, True),
    ("M10", "Inter-Medium.otf", 10, TXT, True),
    ("M9", "Inter-Medium.otf", 9, TXT, True),
]


def build(name, fname, size, chars, tabular):
    font = ImageFont.truetype(os.path.join(FONT_DIR, fname), size)
    adv_digit = max(round(font.getlength(c)) for c in DIG) if tabular else 0
    glyphs = []
    bitmap = bytearray()
    asc = desc = 0
    for ch in sorted(set(chars), key=ord):
        adv = round(font.getlength(ch))
        bbox = font.getbbox(ch, anchor="ls")
        x0, y0, x1, y1 = bbox
        w, h = x1 - x0, y1 - y0
        if ch in DIG and tabular:
            shift = (adv_digit - adv) / 2.0
            adv_out = adv_digit
        else:
            shift, adv_out = 0, adv
        if w <= 0 or h <= 0 or ch == " ":
            glyphs.append((ord(ch), len(bitmap), 0, 0, adv_out, 0, 0))
            continue
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
        off = len(bitmap)
        bitmap += img.tobytes()
        dx = int(round(x0 + shift))
        glyphs.append((ord(ch), off, w, h, adv_out, dx, -y0))
        asc = max(asc, -y0)
        desc = max(desc, y1)
    line = asc + desc + 1
    return glyphs, bitmap, asc, line


def main():
    out = []
    out.append("// Généré par tools/make_fonts.py — ne pas modifier à la main.\n")
    out.append("// Polices : Inter (SIL OFL 1.1), glyphes alpha 8 bits.\n#pragma once\n#include <stdint.h>\n\n")
    out.append("struct GlyphDef { uint16_t code; uint32_t off; uint8_t w, h, adv; int8_t dx, dy; };\n")
    out.append("struct FontDef { const GlyphDef *glyphs; uint16_t n; const uint8_t *bmp; uint8_t ascent, lineH; };\n\n")
    total = 0
    names = []
    for name, fname, size, chars, tab in FONTS:
        glyphs, bmp, asc, line = build(name, fname, size, chars, tab)
        total += len(bmp) + len(glyphs) * 12
        out.append("static const GlyphDef %s_G[] = {\n" % name)
        for g in glyphs:
            out.append("  {%d,%d,%d,%d,%d,%d,%d},\n" % g)
        out.append("};\n")
        out.append("static const uint8_t %s_B[] = {" % name)
        for i, b in enumerate(bmp):
            if i % 32 == 0:
                out.append("\n  ")
            out.append("%d," % b)
        if not bmp:
            out.append("0")
        out.append("\n};\n")
        out.append("static const FontDef FONT_%s = {%s_G, %d, %s_B, %d, %d};\n\n" % (name, name, len(glyphs), name, asc, line))
        names.append(name)
        print("%-4s %2dpx : %3d glyphes, %6d octets, ascent %d, ligne %d" % (name, size, len(glyphs), len(bmp), asc, line))
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("".join(out))
    print("total ~%.1f Ko -> %s" % (total / 1024, os.path.normpath(OUT)))


if __name__ == "__main__":
    main()
