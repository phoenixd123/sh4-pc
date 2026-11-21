#!/bin/sh
# Builds and runs tools/mods_test.c against a folder it creates in build/modtest. No game, no window, no SDL.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${ZIG:-zig}
OUT=$ROOT/build/modtest
rm -rf "$OUT"
mkdir -p "$OUT"
python - "$OUT" <<'PY'
import sys, zipfile, os
out = sys.argv[1]
os.chdir(out)
with zipfile.ZipFile("fixture-mod.zip", "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("fixture-mod/files/data/inside.bin", "inside")
with zipfile.ZipFile("fixture-loose.zip", "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("data/loose.bin", "loose")
with zipfile.ZipFile("fixture-game.zip", "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("SILENT HILL 4.exe", "not a real executable")
    z.writestr("data/room.bin", "room")
PY
"$ZIG" cc -target x86_64-windows-gnu -std=gnu11 -O1 -w -I"$ROOT/src" -o "$OUT/mods_test.exe" \
    "$ROOT/tools/mods_test.c" "$ROOT/src/mods/mods.c" "$ROOT/src/mods/zip.c" "$ROOT/src/mods/stb_impl.c" -lm
cd "$ROOT" && "$OUT/mods_test.exe"
