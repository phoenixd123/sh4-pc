#!/bin/sh
# Turns your own SILENT HILL 4.exe into the C the port compiles, into src/recomp/gen/.
# Needs: Python 3 with Capstone, and re/out/functions.json from Ghidra (step 2 in docs/BUILD-WINDOWS.md).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PY=${PYTHON:-python3}
[ -f "$ROOT/game/sh4.exe" ] || {
    echo "Put your GOG 1.0 gog v5 SILENT HILL 4.exe at game/sh4.exe." >&2
    echo "Its SHA-256 must be cf7081a6c78dbe51f6dc2e1c9bd978d8486f1d7b59d7a3b88e027c9b39f21dd0." >&2
    exit 1
}
[ -f "$ROOT/re/out/functions.json" ] || {
    echo "re/out/functions.json is missing: export the functions with Ghidra first:" >&2
    echo "  analyzeHeadless <project-dir> sh4 -import game/sh4.exe \\" >&2
    echo "    -scriptPath re/ghidra -postScript ExportFunctions.java re/out/functions.json -deleteProject" >&2
    exit 1
}
cd "$ROOT"
"$PY" recomp/x86rec.py
echo "wrote src/recomp/gen: now build with CMake (docs/BUILD-WINDOWS.md) or tools/build-windows.sh"
