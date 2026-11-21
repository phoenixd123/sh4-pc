<div align=center>

<h1>Silent Hill 4: The Room — PC</h1>

<p><b>0.2.0</b> — ported to PC by <b>phoenixd123</b></p>

</div>

A native, statically recompiled port of the PC version of **Silent Hill 4: The Room** for **Windows**. It is not an
emulator: the game's x86 executable is translated to C ahead of time, and that C is compiled for the machine you
build it on. The Windows API the game expects (Direct3D 8, DirectSound, DirectInput, kernel32, user32) is
reimplemented on top of SDL2 and OpenGL.

<p align=center><b><a href="https://github.com/phoenixd123/sh4-pc/releases/latest">Download the Windows launcher</a></b>
(no build needed). It is the launcher only: the game's code is recompiled from your own
<code>SILENT HILL 4.exe</code>, so the full game is built from your copy, not downloaded — see
<a href="docs/BUILD-WINDOWS.md">docs/BUILD-WINDOWS.md</a>.</p>

This is a separate project that started from **[sh4vita](https://github.com/vallfrr/sh4vita)** by vallfrr, the
PlayStation Vita port whose recompiler and runtime are almost everything that makes the game run at all. **Who did
what is in [CREDITS.md](CREDITS.md)**, and the state of every known problem is in **[BUGS.md](BUGS.md)**.

> [!NOTE]
> No game files are included. You need your own copy of the GOG release, and you build the executable yourself from
> it; the recompiler reads your `SILENT HILL 4.exe`.

## What this fork adds to sh4vita

- **A Windows build.** SDL2 window and desktop OpenGL instead of the Vita layer, with a real launcher in front of
  the game: a warning screen, Play, Options, and a credit roll.
- **Options that stick.** `sh4.ini` holds `resolution=`, `fullscreen=` and `vsync=`; the launcher edits them and the
  game window is created to match. `1920x1080` and `1280x720` are 16:9; `1024x768` and `640x480` are the 4:3 sizes.
- **It remembers your game.** The folder is checked (the executable's SHA-256, not its name) and saved in
  `sh4-pc.ini` beside the port, so later launches go straight to the launcher. A path argument, `SH4_DIR` and a
  folder dropped on the window all work, and a PlayStation 2 disc is named as one instead of failing silently.
- **Mods**, installed from the launcher: `mods/<name>/files/…` replaces any file the game opens for reading, with
  `mods/load_order.txt` and `mod.ini` (`name=`, `enabled=`, `priority=`) setting the order. See [MODDING.md](MODDING.md).
- **Screenshots**: **F12** writes the presented frame to `screenshots/sh4-<date>-<time>.png` at the game's own
  resolution. **F11** toggles borderless fullscreen.
- **A Mods screen** (**Mods** in the menu, or **M** anywhere in the launcher): the mods installed from `mods/` on
  the left, and the Nexus Mods list for Silent Hill 4 on the right, straight from the official API with the
  player's own key (`nexus_key=` in `sh4.ini`). **A** downloads and installs a mod, **N** opens its page (the way
  to install without Nexus Premium), **R** refreshes, **B** goes back.
- **Two looks over the whole frame** (`filter=` in `sh4.ini`, or Options > Filter): CRT scanlines with a warm tube
  and darker corners, or a VHS pass with colour bleed, grain and a slow roll. Off by default; a shader that fails
  to build leaves the plain blit alone.
- **Anisotropic filtering** (`aniso=`, default 8, 1 = off), which is what keeps a texture sharp at grazing angles.
- **The game download, imported**: **Options > Import a .zip download** takes a pre-installed copy (a Full-Rip zip
  with `SILENT HILL 4.exe` and `data/` inside), unpacks it beside itself with the launcher's own streaming ZIP
  reader, and makes the folder inside the game. No `tar.exe`, so it works under Proton too. A PlayStation 2 disc
  is still refused, but now offers to open it in PCSX2 when the emulator is installed.
- **Updates from the launcher**: **Options > Check for updates** asks GitHub for the newest release of this
  repository and, with a yes, downloads it and replaces the launcher, `SDL2.dll` and `README.txt` once the
  launcher exits (a one-shot script starts it again).
- **A build that works without the game.** With no generated code, the same tree builds a launcher-only binary —
  the release download — where Play offers to start the copy's own `SILENT HILL 4.exe`.

The recompiled game itself is untouched: it is the same code sh4vita produces, from the same executable. Nothing
has been played through in this fork; see [BUGS.md](BUGS.md).

## Getting it running

1. **Your own game.** Silent Hill 4: The Room from **GOG**, version **"1.0 gog v5"**: the folder holding
   `SILENT HILL 4.exe` beside `data/`, `demo/`, `movie/` and `sound/`. The executable's SHA-256 must be
   `cf7081a6c78dbe51f6dc2e1c9bd978d8486f1d7b59d7a3b88e027c9b39f21dd0`; the launcher checks it and says what it
   found when it is another version.
2. **Build once, from your copy.** There is no binary download of the game: the recompiler turns *your* executable
   into C, so a build belongs to the copy it was made from. Python 3 with Capstone, Ghidra 12 (for the function
   export), and MSYS2's mingw-w64 + SDL2 + CMake (or the bundled zig script, which needs no MSYS2):
   [docs/BUILD-WINDOWS.md](docs/BUILD-WINDOWS.md).
3. **Start it.** `build/windows/sh4-pc.exe`, with `SDL2.dll` beside it. The launcher remembers the folder you pick
   in **Options > Game folder**, and Play starts the port.
4. **Mods are optional**: a `mods/` folder in the game folder, or **Options > Install a mod** with a `.zip`.

### The launcher-only download

The release download, `sh4-pc-launcher-0.2.0-win64.zip`, is the launcher built from this tree with the generated
game code left out; the same zip is what `tools/build-windows.sh` writes into `dist/`.
It checks your copy, remembers it, edits `sh4.ini` and installs mods; **Play starts that copy's own
`SILENT HILL 4.exe`** (the GOG executable), because there is no recompiled code in it. A build made from your own
copy runs the port instead.

## Where the game files come from

Silent Hill 4: The Room was sold on four CDs in 2004 and re-released digitally by GOG. The port recompiles the
**GOG 1.0 gog v5** executable, and only that one: the shapes of its functions are what the recompiler was written
against, and another build's code would translate into something that crashes rather than runs. The launcher pins
the SHA-256 rather than the file name, and names the hash it found when the copy is another release.

**The PlayStation 2 release cannot be used.** It is a MIPS binary with the console's own data formats and hardware;
this port is a recompilation of the PC build. Point the launcher at a PS2 image and it says exactly that, naming
the program the disc boots when it can read one, instead of failing silently.

No disc check, no installer and no optical drive is involved at run time: the port reads the executable as a file
and supplies its own Windows layer.

## Settings

`sh4.ini`, in the game folder (every line optional). The launcher's Options screen writes all of these except
`nexus_key=`, which is the player's own secret:

| Setting | Effect |
| --- | --- |
| `resolution=1280x720` | the default; `1920x1080`, `1366x768`, `1024x768`, `640x480` are offered. The game sets its field of view from the back buffer, so a 16:9 size is the widescreen presentation |
| `fullscreen=0` | `1` starts borderless on the desktop; **F11** toggles it in game |
| `vsync=1` | `0` lets the game present as fast as it can |
| `filter=0` | a look over the whole frame: `0` off, `1` CRT (scanlines, warm tube, darker corners), `2` VHS (colour bleed, grain, slow roll) |
| `aniso=8` | anisotropic filtering, 1-16 (default 8, `1` = off) |
| `nexus_key=` | the player's own Nexus Mods API key, read by the Mods screen. `SH4_NEXUS_KEY` in the environment does the same for a test run |

The port keeps one file of its own beside the executable: `sh4-pc.ini`, holding `game=`, the folder the launcher was
pointed at. Environment variables for debugging: `SH4_DIR` (the game folder, as an argument), `SH4_GUEST_MB` (guest
memory, default 512), `SH4_RES` (what the game is told about the window; the launcher sets it), `SH4_HEADLESS` (no
dialog boxes), `SH4_KEYS` (a scripted key list, for the launcher's own tests).

## Controls

The game's own PC bindings are what the keyboard uses: the port translates SDL scancodes to the DirectInput key
numbers `SILENT HILL 4.exe` asks for, so W A S D, the arrow keys, Return and Escape mean what they do in the 2004
release. A gamepad uses sh4vita's fixed layout (no remapping): left stick moves, right stick turns, ✕ acts,
○ runs/cancels, △ opens items, □ the map, Start pauses, and **L + □** / **R + ○** are the item-info and dodge
combos.

**F11** fullscreen and **F12** screenshots are the port's own. In the launcher, **M** opens the Mods screen from
anywhere, **A** picks and **B** goes back; the D-pad and left stick move, and Start also picks.

## Building

You need the game (step 1 above): the code generator reads `SILENT HILL 4.exe`. The full page is
[docs/BUILD-WINDOWS.md](docs/BUILD-WINDOWS.md).

```sh
python -m venv .venv && .venv/bin/pip install capstone      # the recompiler's disassembler
mkdir -p game && cp "/path/to/SILENT HILL 4.exe" game/sh4.exe

# 1. Ghidra exports the function list (once per copy of the game)
analyzeHeadless <project-dir> sh4 -import game/sh4.exe \
  -scriptPath re/ghidra -postScript ExportFunctions.java re/out/functions.json -deleteProject

# 2. The recompiler writes src/recomp/gen (about 68 MB of C)
tools/regen.sh

# 3a. CMake + mingw-w64 + SDL2
cmake -S . -B build/windows -G Ninja && cmake --build build/windows

# 3b. or zig, with no MSYS2 install
SH4_SDL=/path/to/SDL2-2.30.9/x86_64-w64-mingw32 tools/build-windows.sh
```

Without `src/recomp/gen`, the same commands produce the launcher-only binary.

## Licence

The original code in this repository — the launcher, the mod system, the Windows entry point and the build scripts —
is under the **GNU General Public License v3.0**, because it is built on sh4vita's GPL-3 runtime and recompiler.
Third-party components keep their own licences: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[LICENSES/](LICENSES). Neither licence covers Silent Hill 4.

**Silent Hill 4: The Room** is © Konami. Created by **Team Silent**. This project is not affiliated with, endorsed
by or sponsored by Konami or GOG. Use your own copy; do not ask for game files.

## Not ported from the Silent Hill 3 launcher

Two features of this fork's Silent Hill 3 counterpart have no equivalent here, and are not pretended to:
**freecam (F1)** and the **over-the-shoulder / first-person camera (F2)** hook the view matrix in the render layer.
SH4's renderer hands pre-combined world-view-projection matrices to its own vertex shaders, so there is no single
view matrix to shift without reverse-engineering each shader; doing it half-way would move some geometry and not
the rest. **HD texture packs** are missing for the same kind of reason: SH4 uploads paletted and DXT textures by
offset, and this tree has no keyed texture path to replace them through. Both need the game running to develop
against; [BUGS.md](BUGS.md) says so.
