/* What a path the player picked actually is, decided from its contents. */
#pragma once
#include <stddef.h>

enum {
    SH4_COPY_NONE = 0,
    SH4_COPY_GAME,     /* SILENT HILL 4.exe + data/ and the supported executable */
    SH4_COPY_WRONG_EXE,/* a Silent Hill 4 install, but not the GOG 1.0 gog v5 executable */
    SH4_COPY_PS2,      /* a PlayStation 2 disc image */
    SH4_COPY_MOD,      /* a .zip with no game in it: a mod archive */
    SH4_COPY_DOWNLOAD, /* a .zip holding SILENT HILL 4.exe and data/: a pre-installed game download */
    SH4_COPY_DISC,     /* some other disc or file */
};

/* The folder a copy of the game lives in, with no trailing separator, or NULL when the path is not one.
 * `msg` (when given) says what was found, in words the player reads. */
int sh4_check_copy(const char *path, char *dir, size_t dn, char *msg, size_t mn);

/* The SHA-256 of a file, lowercase hex, or an empty string. */
void sh4_sha256_file(const char *path, char hex[65]);

/* Starts the copy's own SILENT HILL 4.exe in `dir`, detached. 1 when it started. */
int sh4_start_original(const char *dir);

/* A game download: the archive is unpacked beside itself (streamed, never in memory) and the folder inside that
 * holds SILENT HILL 4.exe and data/ is returned. 1 when the copy is ready to use, with a sentence in `msg`. */
int sh4_import_zip(const char *zip, char *dir, size_t dn, char *msg, size_t mn);

/* The PlayStation 2 disc this port cannot run, opened in PCSX2 when the emulator is installed. 1 when it started,
 * 0 when PCSX2 could not be found (the caller already has a message saying what the disc is). */
int sh4_open_ps2(const char *path);
int sh4_pcsx2_found(void);

/* The dialog boxes the launcher's rows use: a folder, a game file or disc, a game download, a mod archive. */
int sh4_pick_folder(const char *start, char *out, size_t n);
int sh4_pick_game_file(const char *start, char *out, size_t n);
int sh4_pick_game_archive(const char *start, char *out, size_t n);
int sh4_pick_archive(const char *start, char *out, size_t n);
int sh4_confirm(const char *title, const char *text);
