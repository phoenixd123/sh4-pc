/* Before the game: the launcher's settings and the one call the entry point makes. */
#pragma once

typedef struct {
    char resolution[24];  /* "WIDTHxHEIGHT", what the game is told through SH4_RES */
    int fullscreen;       /* 1 = borderless desktop fullscreen */
    int vsync;            /* 0 or 1 */
    int filter;           /* 0 off, 1 CRT, 2 VHS */
    int aniso;            /* anisotropic filtering, 1 = off, 16 = the most */
    char nexus_key[128];  /* the player's own Nexus Mods API key */
} Sh4Settings;

extern Sh4Settings g_settings;

void sh4_settings_load(void);  /* sh4.ini beside the executable */
void sh4_settings_save(void);

/* Runs the warning screen, the menu and Options. Returns when Play is chosen in a build that has the recompiled
 * game code; a launcher-only build starts the copy's own executable instead and exits. */
void launcher_run(void);

/* A file or folder dropped on the launcher window. */
void launcher_drop(const char *path);
