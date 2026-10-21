#!/bin/sh
# Compile le simulateur d'interface et produit des PNG (×3) dans le dossier donné.
# Usage : CYD/tools/sim/build.sh [dossier_de_sortie]   (nécessite g++ et python3 + Pillow)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$HERE/out}"
mkdir -p "$OUT"
g++ -std=gnu++11 -O1 -Wall -Wextra -I"$HERE" -I"$HERE/../../src/dashboard" "$HERE/sim.cpp" -o "$OUT/sim"
"$OUT/sim" "$OUT"
python3 - "$OUT" <<'PY'
import sys, glob, os
from PIL import Image
out = sys.argv[1]
for f in sorted(glob.glob(os.path.join(out, "*.ppm"))):
    Image.open(f).resize((960, 720), Image.NEAREST).save(f[:-4] + ".png")
    os.remove(f)
    print(f[:-4] + ".png")
PY
