#!/bin/sh
# Windows build with zig cc: no MSYS2 needed. Produces build/windows/sh4-pc.exe and its SDL2.dll.
# The game's C code (src/recomp/gen/) comes from your own SILENT HILL 4.exe (tools/regen.sh); without it
# this builds the launcher only, which is what the release zip is.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${ZIG:-zig}
SDL=${SH4_SDL:-$ROOT/.tools/SDL2-2.30.9/x86_64-w64-mingw32}
OUT=$ROOT/build/windows
GEN=$ROOT/src/recomp/gen
[ -d "$SDL" ] || { echo "SDL2 for mingw-w64 not found: set SH4_SDL to a SDL2-*/x86_64-w64-mingw32 folder" >&2; exit 1; }
mkdir -p "$OUT" "$GEN"

INC="-I $ROOT/src -I $ROOT/src/rt -I $ROOT/src/win32 -I $ROOT/src/d3d8 -I $ROOT/src/dx -I $GEN"
INC="$INC -I $ROOT/src/third_party -I $ROOT/src/third_party/miniz -I $SDL/include -I $SDL/include/SDL2"
DEFS="-DPORT_AUTHOR=\"${PORT_AUTHOR:-phoenixd123}\" -DPORT_NAME=\"SH4-PC\""
DEFS="$DEFS -DSRC_DIR=\"$(cd "$ROOT" && pwd -W 2>/dev/null || pwd)\" -DSDL_MAIN_HANDLED -D_CRT_SECURE_NO_WARNINGS"
[ -n "$PORT_LAUNCHER_ONLY" ] && DEFS="$DEFS -DPORT_LAUNCHER_ONLY"
CFLAGS="-std=gnu11 -O2 -g0 -fwrapv -fno-strict-aliasing -Wno-macro-redefined -Wno-unused-label"
CFLAGS="$CFLAGS -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-conversion"
LIBS="-lmingw32 $SDL/lib/libSDL2.dll.a -lopengl32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid"
LIBS="$LIBS -ladvapi32 -lshell32 -lcomdlg32 -luser32 -lgdi32 -lsetupapi -ldinput8 -lwinhttp"

n=0
rm -f "$GEN"/stub.c "$OUT"/gen_stub.o "$OUT"/zz_stub.o
[ -f "$STUB" ] && rm -f "$STUB"
for f in "$ROOT"/src/rt/*.c "$ROOT"/src/win32/*.c "$ROOT"/src/d3d8/*.c "$ROOT"/src/dx/*.c \
         "$ROOT"/src/pc/*.c "$ROOT"/src/mods/*.c "$ROOT"/src/third_party/tlsf/*.c "$ROOT"/src/third_party/miniz/*.c "$GEN"/*.c; do
  [ -f "$f" ] || continue
  case "$(basename "$f")" in stub.c) continue;; esac
  o="$OUT/$(basename "$(dirname "$f")")_$(basename "$f" .c).o"
  case "$f" in *"$GEN"*) o="$OUT/gen_$(basename "$f" .c).o";; esac
  "$ZIG" cc -target x86_64-windows-gnu $CFLAGS $INC $DEFS -c "$f" -o "$o"
  n=$((n + 1))
done
echo "compiled $n translation units"

# Generated game code is never committed: link against stubs until every symbol it would define is found.
STUB=$OUT/stub.c   # never in gen/: CMake globs that folder as game code
echo '#include <stdint.h>' > "$STUB"
for round in 1 2 3 4 5 6 7 8; do
  "$ZIG" cc -target x86_64-windows-gnu $INC -c "$STUB" -o "$OUT/zz_stub.o"
  if "$ZIG" cc -target x86_64-windows-gnu -Wl,--subsystem,windows -o "$OUT/sh4-pc.exe" "$OUT"/*.o $LIBS 2> "$OUT/link.txt"; then
    cp "$SDL/bin/SDL2.dll" "$OUT/" 2>/dev/null || true
    echo "linked -> $OUT/sh4-pc.exe"
    exit 0
  fi
  syms=$(grep -o 'undefined symbol: [A-Za-z0-9_]*' "$OUT/link.txt" | sed 's/.*: //' | sort -u)
  [ -z "$syms" ] && { echo "LINK FAILED:"; head -30 "$OUT/link.txt"; exit 1; }
  for s in $syms; do echo "void $s(void) { }" >> "$STUB"; done
  echo "round $round: stubbed $(echo "$syms" | wc -l) symbols"
done
echo "link still failing:"; head -30 "$OUT/link.txt"; exit 1
