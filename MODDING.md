# Mods

A mod is a folder of files that the game reads instead of its own. Nothing is patched in memory and nothing is
rewritten: the emulated `CreateFileA` and `GetFileAttributesA` ask the mod list first, so a file the game opens for
reading comes from the mod when the mod has it.

```
<game folder>/
  SILENT HILL 4.exe
  data/  demo/  movie/  sound/
  mods/
    load_order.txt          (optional)
    hd-models/
      mod.ini               (optional)
      files/
        data/...            (the same relative path the game opens)
```

## The layout

| Path | Meaning |
| --- | --- |
| `mods/<name>/files/<path>` | replaces the game file at `<path>`, e.g. `mods/hd-models/files/data/chr/...` |
| `mods/<name>/mod.ini` | `name=` (what the log calls it), `enabled=` (`0` to switch it off), `priority=` (higher wins) |
| `mods/load_order.txt` | one mod folder per line, first line wins; lines starting with `#` or `;` are comments |

Order: `load_order.txt` first, then unlisted mods by `priority` (descending), then by folder name. The first mod
with the file wins; later ones are not consulted for it.

A folder counts as a mod if it holds `files/` or a `mod.ini`. A folder with neither is ignored, so a stray
directory in `mods/` does nothing.

## Installing one

**Options > Install a mod** in the launcher takes a `.zip`:

- an archive that already holds `files/` or `mod.ini` is the mod as it stands;
- an archive with everything inside one folder (`MyMod/data/...`) is that folder;
- loose game paths (`data/...`, `movie/...`) are wrapped in `files/`, which is the path the game reads through.

It unpacks through `mods/.install`, refuses an entry that would land outside the folder, refuses a name that is
already taken, and loads the mod immediately (it applies to the game started after the menu). Dropping the archive
on the launcher window does the same.

## The Mods screen, and Nexus Mods

**Mods** in the launcher's menu (or **M** anywhere in it) shows two columns: the mods installed from `mods/` on the
left, and the Nexus Mods list for Silent Hill 4 on the right. The list comes from the official API
(`api.nexusmods.com`), not from scraping, and needs your own personal key in `sh4.ini`:

```ini
nexus_key=your-key-here
```

The key is yours and stays on your machine; it is not part of this repository. `SH4_NEXUS_KEY` in the environment
does the same for a test run.

- **A** downloads the selected mod's main file and installs it into `mods/`, in the background, with progress.
  The API only allows automatic downloads for **Premium** accounts: a free key can list mods, but the download
  answers 403. The launcher says so and points at the mod's page instead of working around it.
- **N** opens the selected mod's page. That is the route without Premium: download the file there and drop the
  `.zip` on the launcher window; it is installed like any other archive.
- **R** asks Nexus for the list again; **B** goes back. A `.zip` dropped on the window works from any screen.

## What can be replaced

Anything the game opens by path: the `.sh4` room archives, the `.pac` sound archives, textures, models, movies.
Two practical notes:

- The file must be a *replacement*: same format, and whatever internal offsets the game expects still valid.
  Shipping a repacked archive is the usual way.
- A mod that ships a path the game also *writes* (a save, a config) makes the game read the mod's copy. Keep mods
  to data the game only reads.

Nothing here contains or derives from the game's assets. A mod is made by a player, from their own copy, and
shared at their own discretion.
