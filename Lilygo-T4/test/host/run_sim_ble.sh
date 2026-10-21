#!/bin/sh
# Comme run_sim.sh, pour la variante Bluetooth LE (-DPHONE_LINK_BLE=1) : captures d'écran PNG dans le dossier donné.
# Usage : Lilygo-T4/test/host/run_sim_ble.sh [dossier_de_sortie]   (nécessite g++ et python3 + Pillow)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-${TMPDIR:-/tmp}/t4-sim-ble}"
mkdir -p "$OUT"
g++ -std=gnu++11 -O1 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -DPHONE_LINK_BLE=1 \
    -I"$HERE/shim" -I"$HERE/../../src/dashboard" -I"$HERE/../../../common/src" \
    "$HERE/sim_ble_main.cpp" "$HERE/../../src/dashboard/main.cpp" -o "$OUT/sim"
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
