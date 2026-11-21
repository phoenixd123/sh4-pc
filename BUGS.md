# Bugs

What is known against this port, and what a fix needs. "Fixed here" means the defect was found in this tree and
corrected; "open" means it needs the game running to diagnose, and the next step is written down.

## Fixed in this fork

| Was | What was wrong | Fix |
| --- | --- | --- |
| The warning screen ignored the first press | The "Press A" loop required a key *and* the end of its fade, so a press made during the fade was consumed and thrown away; the screen then waited for a second one. | The press is remembered (`pressed`), and the loop leaves as soon as the fade is done. |
| The window was always 640x480 | The desktop layer was sh4vita's test harness: one fixed size, and the game told about it through the environment only. | The window is created from `sh4.ini`, and `SH4_RES` is set from the same value so the game's field of view and menus match. |
| Play in a build without game code fell into stub functions | `gen/` generates the game's functions; without it the link was satisfied by empty stubs. | `CMakeLists.txt` and `tools/build-windows.sh` build the launcher-only variant (Play starts the copy's own executable) instead of a port that cannot run. |

## Open

### 1. The port has not been played

This tree was written without a copy of the game on the machine. What is verified: the runtime and launcher compile
(x86-64, zig cc and mingw-w64), the launcher-only binary runs, its menus write `sh4.ini`, and the copy check
identifies an executable by hash. What is **not** verified: the recompiled game running, the D3D8 → GL path on a
desktop driver, audio, movies, mods being picked up by the game, and the F12 screenshot path.

**What a fix needs:** build from your own copy and play it. A failure writes `sh4log.txt` in the game folder (the
previous 19 launches are kept beside it), which names the last Windows calls and any fatal; that is what makes the
next step decidable.

### 2. Mods are not exercised in game

`mods/<name>/files/...` is consulted by `w32_host_path`, the function every emulated file open and attribute query
goes through, so a mod file stands in for the game's own at the same relative path. That mapping is all that could
be tested here; a mod that the game reads through a path the bridge never sees (an archive read by offset, for
instance) would be missed.

**What a fix needs:** a mod replacing one file the game loads early (a room or a model archive), `mods/` enabled,
and a log line per override. `rt_log` already names the resolution; what is missing is the proof that the game
opens the overridden path.

### 3. Continue on the title screen stays greyed out

Inherited from sh4vita: the title's **Continue** is inert; **Load** resumes a save. Not a regression here.

### 4. Heavy scenes can drop below 30 fps

The game logic runs at 30 Hz; rendering is interpolated toward 60 where the machine can afford it. Sh4vita measured
this on a Vita; a desktop GPU has far more headroom, but no frame timings have been taken here, so no claim is made.

### 5. No widescreen switch

SH4 sizes its field of view and its 2D from the back buffer, so a 16:9 `resolution=` is the widescreen presentation
and a 4:3 size is the original one. There is no separate toggle to restore 4:3 camera framing at a 16:9 resolution,
because the game has no such switch to drive.

### 6. Photosensitivity warning and Konami logos

They are the game's own screens, drawn by the recompiled code; nothing in this fork changes them.

### 7. Saves

Saves are the game's own files in its folder (`save/`), written through the Win32 bridges. Nothing migrates them
between versions, because the format is the game's; keep a copy before switching builds.

### 8. Freecam and the camera modes are not ported

The Silent Hill 3 port's **F1 freecam** and **F2 over-the-shoulder / first-person camera** shift the view matrix at
`SetTransform`, which works because that renderer applies the view itself. SH4's game code builds a combined
world-view-projection and uploads it in its vertex-shader constants; `Device.view` is only the fixed-function
path, so shifting it would move some draws and not others. Doing this properly needs the game's shader constants
identified while it runs.

**What a fix needs:** a trace of the constant registers each vertex shader reads for a frame (the port already
logs shader source and constants), then the matrix registers found by comparing two frames with a known camera
move. Until then the feature is absent rather than half-working.

### 9. HD texture packs are not supported

Mods replace game files by path (`mods/<name>/files/...`), which is enough for archives, models and scripts. The
Silent Hill 3 port also replaced individual textures by a key over the pixels uploaded; SH4's texture path is
paletted/DXT and keyed by archive offset, and this tree never computes such a key. **What a fix needs:** a dump
hook in `tex_upload` that expands the formats the game uses, a stable key (level 0 bytes, format, size and
palette) and a lookup before upload. It needs the game to produce a corpus to test against.

### 10. The Nexus screen and the updater are not verified against the live services

Both were built and compiled, and their failure paths are written, but this machine has no Nexus account and no
published release to update from. **What a fix needs:** a personal `nexus_key=` for the list, a Premium account
for the download path, and a tagged release whose asset is named `...win64.zip` for the updater. The Nexus game
domain used is `silenthill4`; if the site's domain differs the list answers 404 and the code says so.

### 11. The filters and anisotropic filtering are not seen running

They compile into the Windows build and their settings round-trip, but no frame has been drawn through them here
(no game). **What a fix needs:** any scene with `filter=1` or `filter=2` in `sh4.ini`, and a driver without
`GL_EXT_texture_filter_anisotropic` to confirm `aniso=` leaves rendering unchanged.
