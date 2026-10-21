#!/bin/sh
# Compile et lance les tests de la logique pure de la T4 sur PC (aucun matériel, aucune bibliothèque Arduino).
# Usage : Lilygo-T4/test/host/run.sh      (nécessite g++)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${TMPDIR:-/tmp}/t4-logic-test"
g++ -std=gnu++11 -O1 -Wall -Wextra -Wno-unused-function \
    -I"$HERE/../../src/dashboard" -I"$HERE/../../../common/src" \
    "$HERE/logic_test.cpp" -o "$OUT"
"$OUT"
