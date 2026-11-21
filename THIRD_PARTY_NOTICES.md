# Third-party notices — SH4-PC

The port uses third-party software. Its authors retain their copyrights and the terms below. These notices do not
claim ownership of Silent Hill 4, its executable, its resources, or any translation generated from it: players
supply their own supported GOG installation.

## The port this is built on

- **sh4vita**, vallfrr and contributors: [source](https://github.com/vallfrr/sh4vita), **GPL-3.0**
  (`LICENSES/GPL-3.0.txt`). The recompiler (`recomp/`), the Win32/Direct3D 8/DirectInput/DirectSound replacements
  (`src/rt`, `src/win32`, `src/d3d8`, `src/dx`), the DirectShow movie path and the game-side fixes are sh4vita's,
  mostly unchanged; `src/pc/` replaces its Vita layer. This repository's own code is therefore GPL-3.0 too.

## Bundled and linked support libraries

- **pl_mpeg**, Dominic Szablewski: [source](https://github.com/phoboslab/pl_mpeg), MIT (`LICENSES/MIT.txt`,
  `src/third_party/pl_mpeg/NOTICE.md`). The bundled decoder carries sh4vita's modifications, including MPEG audio
  Layer I support; it is not an unchanged upstream copy.
- **TLSF**, Copyright (c) 2006–2016 Matthew Conte: [source](https://github.com/mattconte/tlsf), BSD 3-clause
  (`LICENSES/TLSF-BSD-3-Clause.txt`), used for the guest heap.
- **SDL2**, Sam Lantinga and contributors: [source](https://github.com/libsdl-org/SDL), zlib
  (`LICENSES/SDL2-Zlib.txt`), linked as a DLL: window, input, audio, threads and clocks.
- **stb**, Sean Barrett: [source](https://github.com/nothings/stb), public domain / MIT — the PNG writer behind F12
  screenshots and the zlib decoder the mod installer unpacks archives with.
- **miniz**, Rich Geldreich, Valve Software and contributors: [source](https://github.com/richgel999/miniz), MIT —
  the ZIP reader used by the launcher's self-updater (`src/third_party/miniz`).

## Build and reverse-engineering tools (not distributed)

- **Capstone**: [source](https://github.com/capstone-engine/capstone), BSD-3 — the recompiler's disassembler.
- **Ghidra**: [source](https://github.com/NationalSecurityAgency/ghidra), Apache-2.0 — exports the function list the
  recompiler translates. The generated C is produced on the player's machine from their own executable and is
  never distributed.
- **mingw-w64 / zig**: the compilers the Windows build uses.

`LICENSES/` holds the texts of the components above. The game itself is not covered by any of them.
