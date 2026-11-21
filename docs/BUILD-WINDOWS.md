# Building on Windows

The port is C: the game's code, generated from your own `SILENT HILL 4.exe`, compiled with the runtime in `src/`.
Two things are needed before a build: the game's executable, and its function list from Ghidra.

What you need:

| Step | Tools |
| --- | --- |
| Function export | [Ghidra](https://ghidra-sre.org/) 12 and JDK 21 |
| Recompiler | Python 3 with [Capstone](https://www.capstone-engine.org/) (`pip install capstone`) |
| C compiler | MSYS2's mingw-w64 (`pacman -S mingw-w64-x86_64-gcc`) **or** [zig](https://ziglang.org/) 0.13 |
| SDL2 | MSYS2's `mingw-w64-x86_64-SDL2`, or a released `SDL2-devel-*-mingw.tar.gz` |
| Build system | CMake 3.20+ and Ninja (MSYS2), or `tools/build-windows.sh` with zig |

## 1. Put the game in place

```sh
mkdir -p game
cp "/c/Program Files (x86)/GOG Galaxy/Games/Silent Hill 4/SILENT HILL 4.exe" game/sh4.exe
```

The executable must be the **GOG 1.0 gog v5** one, SHA-256
`cf7081a6c78dbe51f6dc2e1c9bd978d8486f1d7b59d7a3b88e027c9b39f21dd0`. The port refuses another build at run time;
the recompiler would happily translate it into code that does not work. Check it with:

```sh
sha256sum game/sh4.exe
```

## 2. Export the functions with Ghidra

Ghidra finds the functions; the recompiler translates the ones it lists.

```sh
analyzeHeadless <project-dir> sh4 -import game/sh4.exe \
  -scriptPath re/ghidra -postScript ExportFunctions.java re/out/functions.json -deleteProject
```

This takes a few minutes and writes `re/out/functions.json`. It is per copy of the game, like the generated code.

## 3. Recompile

```sh
python -m venv .venv
.venv/bin/pip install capstone
PYTHON=.venv/bin/python tools/regen.sh
```

`regen.sh` reads `game/sh4.exe` and `re/out/functions.json` and writes `src/recomp/gen/` — about 68 MB of C, one
function per game function. It is never versioned: it is the game's code.

## 4. Build

With MSYS2 (from the MSYS2 shell, `mingw64` environment):

```sh
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/windows
```

`SDL2.dll` must sit beside the executable; CMake's SDL2 package knows where it is, the copy is manual:

```sh
cp "$(dirname "$(which sdl2-config)")/../bin/SDL2.dll" build/windows/
```

Or with zig, which needs no MSYS2 install:

```sh
SH4_SDL=/c/SDL2-2.30.9/x86_64-w64-mingw32 tools/build-windows.sh
```

The script compiles every translation unit, links as a Windows program, and copies `SDL2.dll` next to the result:
`build/windows/sh4-pc.exe`.

## The launcher-only build

Without `src/recomp/gen/` the same commands build the launcher only. `CMakeLists.txt` notices the empty folder and
defines `PORT_LAUNCHER_ONLY`; the build script does the same. In that build **Play** offers to start the copy's own
`SILENT HILL 4.exe`, which is what the release zip is for: it needs no generated code, because none may be
distributed.

## What the port does differently from the Vita build

- `src/vita/` is not compiled; `src/pc/` supplies the same functions: the window, the message box, event pumping,
  GL context creation, swap and drawable size.
- The window is created from `sh4.ini`, at the resolution the launcher chose, and `SH4_RES` is set from the same
  value. The menu bar, the field of view and the 2D layout all follow the back buffer.
- The log is `sh4log.txt` in the game folder, rotated (the previous 19 launches kept as `.01`…`.19`), and no
  console appears behind the launcher: the executable is linked as a Windows program.
- `SH4_GUEST_MB` defaults to 512, which the game needs for its textures and streams; it is a desktop, not a Vita.

## Troubleshooting

| Symptom | Cause |
| --- | --- |
| "cannot load …SILENT HILL 4.exe" | the game folder is wrong, or the executable is not where the launcher thinks |
| "This is Silent Hill 4, but not the release this port recompiles" | the SHA-256 differs: install GOG 1.0 gog v5 |
| A black window for a few seconds, then the launcher | normal: the launcher fades in from black |
| `sh4log.txt` says the game's code is missing | a launcher-only build; rebuild with `src/recomp/gen/` present |
| The linker cannot find `SDL2.dll.a` | `SH4_SDL` (build script) or SDL2's CMake package (CMake) points at the wrong folder |
| `undefined symbol: F_...` at link time with CMake | `gen/` was regenerated after configuring: re-run CMake |
