# Credits

**Silent Hill 4: The Room — PC** is an unofficial, fan-made port. It contains no assets or program code from the
game; you need your own copy of Silent Hill 4: The Room for PC (GOG, 1.0 gog v5).

## This port

**phoenixd123** (@phoenixdobsonyc on Instagram) is the only contributor to this port: the PC platform layer
(`src/pc/`), the launcher and its Mods/Options screens, the filters and anisotropic filtering, the game-folder,
copy and download handling, the mod system, screenshots, the build scripts and this documentation.

The projects under **Standing on** are credited because their code is what this port is built from. They are not
contributors to this repository, and their inclusion does not make them authors of this fork's work.

The name on the credit roll comes from the build: `cmake -DPORT_AUTHOR="Your Name"` (it defaults to
`phoenixd123`), and `PORT_NAME` sets the title under the menu.

## Standing on

- **[sh4vita](https://github.com/vallfrr/sh4vita) by vallfrr** (GPL-3.0) — the port this started from, and almost
  everything in it: the x86 → C recompiler (`recomp/`), the Win32, Direct3D 8, DirectInput and DirectSound
  replacements, the DirectShow movie path, the D3D8 → OpenGL renderer and shader translation, the game-side
  fixes — and the Vita layer this fork replaces with a desktop one. Without it there is nothing here.
- **[Capstone](https://www.capstone-engine.org/)** (BSD-3) — the disassembler the recompiler decodes with.
- **[Ghidra](https://ghidra-sre.org/)** (Apache-2.0) — exports the function list the recompiler translates.
- **[SDL2](https://www.libsdl.org/)** (zlib) — window, input, audio device, threads and clocks.
- **[pl_mpeg](https://github.com/phoboslab/pl_mpeg) by Dominic Szablewski** (MIT), with sh4vita's added MPEG audio
  Layer I decoder — the FMVs.
- **[TLSF](https://github.com/mattconte/tlsf) by Matthew Conte** (BSD-3) — the guest heap.
- **[stb](https://github.com/nothings/stb) by Sean Barrett** (public domain) — the PNG writer behind F12
  screenshots and the zlib decoder the mod installer unpacks archives with.
- **[miniz](https://github.com/richgel999/miniz)** by Rich Geldreich and contributors (MIT) — the ZIP reader the
  launcher's self-updater unpacks its release with.
- **sh4vita's testers**, who played the Vita port and sent the logs its fixes came from.

## Licences

This repository's own code is **GPL-3.0** ([LICENSE](LICENSE)), because it is built on sh4vita's GPL-3 runtime and
recompiler. Third-party components keep their own licences: their texts are in [LICENSES/](LICENSES) and the list is
in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

**Silent Hill 4: The Room is Konami's.** No game code, data or assets are included in this repository or licensed by
it.

## The game

**Silent Hill 4: The Room** is © Konami Digital Entertainment. Created by **Team Silent**: Suguru Murakoshi
(director), Akira Yamaoka (sound and music), Masahiro Ito (art), and the rest of the team. Silent Hill is a
trademark of Konami. This project is not affiliated with, endorsed by or sponsored by Konami or GOG.
