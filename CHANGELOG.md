# Changelog

## 0.2.0 — the launcher services from the Silent Hill 3 port

The find/launch/mod half of this fork's Silent Hill 3 launcher, brought over to SH4 where the renderer allows it.

### Added

- **CRT and VHS filters** (`filter=0/1/2` in `sh4.ini`, **Options > Filter**), drawn by a GLSL 1.20 pass in
  `src/d3d8/filter.c` over the same letterboxed viewport the plain blit used. A shader that fails to build leaves
  the plain blit in place.
- **Anisotropic filtering** (`aniso=`, default 8, **Options > Anisotropic filtering**), applied in the sampler with
  the extension checked once; a driver without it keeps the game's own filtering.
- **The Mods screen** (**Mods** in the menu, **M** anywhere): installed mods from `mods/` and the Nexus Mods list
  for Silent Hill 4 from `api.nexusmods.com`, using the player's own `nexus_key=` in `sh4.ini`. A downloads and
  installs, N opens the page, R refreshes, B goes back. Automatic downloads are a Nexus Premium API feature; a
  free key is told that and pointed at the page, which is not worked around.
- **Options > Import a .zip download**: a pre-installed copy (a Full-Rip zip with `SILENT HILL 4.exe` and `data/`)
  is unpacked beside itself by a new streaming ZIP reader (`zip_extract_file`, so a multi-gigabyte archive is never
  held in memory) and the folder inside it becomes the game. No `tar.exe`, which also makes it work under Proton.
- **PlayStation 2 discs now offer PCSX2** when it is installed (`%LOCALAPPDATA%\Programs\PCSX2`,
  `%ProgramFiles%\PCSX2`, then PATH). Without it, the message is unchanged and still says the port cannot run MIPS
  code. This is where the console release is actually playable, not here.
- **Options > Check for updates** and `src/pc/updater.c`: the newest GitHub release of `phoenixd123/sh4-pc`, its
  win64 launcher zip, and a one-shot script that puts the launcher, `SDL2.dll` and `README.txt` in place after the
  launcher exits and starts it again. `SH4_UPDATE_API` and `SH4_UPDATE_DRYRUN` exist for local testing.
- `sh4.ini` gained `filter=`, `aniso=` and `nexus_key=`; saving from Options preserves every other line.
- **Tests**: the ZIP reader's new file-list sniff and streaming extraction are covered by `tools/test-mods.sh`
  (17 assertions).

### Tested

- The Windows build (zig cc, launcher-only) compiles and links 62 translation units.
- `tools/test-mods.sh`: all assertions pass, including a fixture game download recognised by its file list and
  streamed out of the archive.
- The launcher runs headless with scripted keys, enters Options, changes **Filter** to CRT, saves, and writes
  `filter=1` (plus `aniso=8`) to `sh4.ini`; it exits cleanly.

### Not tested

- The filters, anisotropic filtering, the Nexus screen and the updater have not been run against a game or the
  live services here: there is no copy of SH4 on this machine and no Nexus account. The Nexus API path and the
  update download are exercised only by their code paths, not by a live request.
- **Freecam, the camera modes and HD texture packs are not in this release.** SH4's renderer passes combined
  matrices inside its vertex-shader constants (there is no view matrix to shift) and uploads textures by offset
  with no keyed path; both need the game to develop against. See README, "Not ported".

## 0.1.0 — the first PC build

sh4vita is a PlayStation Vita port. This release is the same recompiled game with the Vita layer replaced by a
desktop one, and a launcher in front of it.

### Added

- **Windows (x86-64).** `src/pc/` replaces `src/vita/`: an SDL2 window at the chosen resolution, a desktop OpenGL
  context created lazily by the renderer, keyboard input translated to the DirectInput key numbers the game asks
  for, and a gamepad through SDL's controller API. The game's own D3D8 → OpenGL layer, DirectSound, DirectInput and
  Win32 bridges are sh4vita's, unchanged.
- **A launcher**: a warning screen every launch starts with, then **Play**, **Options** and **Credits**. It is drawn
  on the CPU into a 1280x720 canvas and scaled to the window, so it needs no shaders and hands the game a clean GL
  state. The images are built into the binary (`assets/*.bmp`, drawn by `tools/launcher_assets.py`).
- **Options**, written to `sh4.ini` in the game folder: resolution (720p, 1080p, 1366x768, 1024x768, 640x480),
  borderless fullscreen, and vsync. The window is created from them at Play, and the game is told the same size
  through `SH4_RES`, which is what its field of view and its menu layout read.
- **The game folder is asked for once.** Options holds **Game folder**, **Game file or disc** and **Check this
  copy**, using the shell's own dialogs, and drops on the window are recognised too. The folder is saved in
  `sh4-pc.ini` beside the port.
- **A copy check that names what it found**: a folder is playable only when `SILENT HILL 4.exe` hashes to the GOG
  "1.0 gog v5" executable (`cf7081a6…21dd0`); another build's hash is printed instead of a guess, a PlayStation 2
  disc is named (with its boot program when the image can be read), and a `.zip` is offered as a mod.
- **Mods**: `mods/<name>/files/...` overrides any file the game opens for reading, through the same host-path
  function the Win32 file calls use; `mods/load_order.txt` and `mod.ini` set the order. **Options > Install a mod**
  takes a `.zip`, unpacks it through a staging folder, works its shape out (`files/` or `mod.ini` as it stands, one
  wrapper folder, or loose `data/...`) and loads it without a restart. [MODDING.md](MODDING.md).
- **F12 screenshots** (`screenshots/sh4-<date>-<time>.png`, at the game's resolution) and **F11** fullscreen.
- **A launcher-only build**, used for the release zip: with no generated code the same tree links a binary whose
  Play starts the copy's own `SILENT HILL 4.exe`. A build from your own copy runs the port.
- **Build scripts**: `tools/regen.sh` (executable → `src/recomp/gen`) and `tools/build-windows.sh` (zig cc, no
  MSYS2). `CMakeLists.txt` builds both shapes and defines `PORT_LAUNCHER_ONLY` itself when `gen/` is empty.
- **Documentation**: [README.md](README.md), [docs/BUILD-WINDOWS.md](docs/BUILD-WINDOWS.md),
  [MODDING.md](MODDING.md), [CREDITS.md](CREDITS.md), [BUGS.md](BUGS.md).

### Fixed

- The launcher's warning screen used to drop a press made while "Press A" was still fading in, and then wait for a
  second one. It remembers the press.
- The window used to be created at the port's 640x480 default. It now follows `sh4.ini`.

### Changed

- The recompiler writes `src/recomp/gen/` instead of `port/gen/`, so one tree holds the port and its generated code
  (which is never versioned).
- The log rotates in the game folder (`sh4log.txt`, with the previous launches kept as `sh4log.txt.01`…), as on the
  Vita.

### Not in this release

- **No game code.** The release zip is the launcher only; the executable is built from your own copy, because it is
  that copy's code.
- **Nothing has been played through here.** This tree was written on a machine without the game: the launcher was
  built and driven, the runtime compiles, and that is the extent of what was checked. [BUGS.md](BUGS.md) says what
  each remaining item needs.
- No shader cache bundle, no texture packs and no button remapping: the Vita port's shader cache is built by its own
  tooling, and SH4 has no hash-keyed texture path here.
