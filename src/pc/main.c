/* Desktop entry point: the launcher, then SDL2 + OpenGL, the game's executable loaded into guest memory, and the
 * recompiled code started. This is the layer sh4vita's Vita main.c is for; everything the game itself does is the
 * same code. */
#include "rt.h"
#include "plat.h"
#include "launcher.h"
#include "copy_check.h"
#include "../mods/mods.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#ifndef PORT_NAME
#define PORT_NAME "SH4-PC"
#endif
#define chdir _chdir
#else
#include <unistd.h>
#endif

static SDL_Window *g_win;
static SDL_GLContext g_gl;
static int g_fullscreen;

void sh4_shot_request(void);              /* pc/screenshot.c */
void plat_drawable_size(int *w, int *h);  /* below, declared here for plat_width/plat_height */

SDL_Window *plat_sdl_window(void) { return g_win; }
int plat_filter_mode(void) { return g_settings.filter; }
int plat_aniso(void) { return g_settings.aniso; }
int plat_width(void) { int w = 0, h = 0; plat_drawable_size(&w, &h); return w; }
int plat_height(void) { int w = 0, h = 0; plat_drawable_size(&w, &h); return h; }
void plat_show_window(void) { if (g_win) SDL_ShowWindow(g_win); }

int plat_message_box(const char *title, const char *text, unsigned type) {
    /* The game's own calls are MB_OK startup errors. Do not fabricate acceptance of a confirmation. */
    if ((type & 15u) || getenv("SH4_HEADLESS")) return 0;
    unsigned icon = (type & 0xf0u) == 0x10u ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION;
    return SDL_ShowSimpleMessageBox(icon, title, text, g_win) == 0 ? 1 : 0;
}

static void toggle_fullscreen(void) {
    if (!g_win) return;
    g_fullscreen = !g_fullscreen;
    SDL_SetWindowFullscreen(g_win, g_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

int plat_pump_events(void) {
    SDL_Event e;
    int quit = 0;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) quit = 1;
        else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_F12 && !e.key.repeat) sh4_shot_request();
            if (e.key.keysym.sym == SDLK_F11 && !e.key.repeat) toggle_fullscreen();
        }
    }
    return quit;
}

void plat_gl_init(void) {
    if (g_gl) return;  /* the launcher ran on the CPU: one context, made here */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    g_gl = SDL_GL_CreateContext(g_win);
    if (!g_gl) rt_fatal("OpenGL context: %s", SDL_GetError());
    SDL_GL_SetSwapInterval(g_settings.vsync ? 1 : 0);
}
void plat_gl_swap(void) { SDL_GL_SwapWindow(g_win); }
void plat_drawable_size(int *w, int *h) {
    if (g_win) SDL_GL_GetDrawableSize(g_win, w, h);
    else *w = *h = 0;
}

void rt_platform_fatal(const char *msg) {
    if (getenv("SH4_HEADLESS")) return;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "sh4-pc", msg, g_win);
}

/* The one program the port recompiles, as the GOG installer writes it. */
static int load_and_run(const char *dir) {
    unsigned guest_mb = 512;
    const char *limit = getenv("SH4_GUEST_MB");
    if (limit) {
        char *end = NULL;
        unsigned long parsed = strtoul(limit, &end, 10);
        if (end != limit && !*end && parsed >= 64 && parsed <= 2048) guest_mb = (unsigned)parsed;
    }
    snprintf(g_game_dir, sizeof g_game_dir, "%s", dir);
    if (chdir(g_game_dir)) rt_fatal("cannot enter %s", g_game_dir);
    sh4_settings_load();  /* filter, aniso, resolution: also with SH4_SKIP_LAUNCHER */

    int w = 1280, h = 720;
    sscanf(g_settings.resolution, "%dx%d", &w, &h);
    g_fullscreen = g_settings.fullscreen;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN |
                   (g_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE);
    char title[128];
    snprintf(title, sizeof title, "Silent Hill 4: The Room (%s)", PORT_NAME);
    g_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
    if (!g_win) rt_fatal("window: %s", SDL_GetError());
    /* The game sizes its field of view and its menus from the back buffer; SH4_RES is what it reads. */
    SDL_setenv("SH4_RES", g_settings.resolution, 1);

    if (rt_init(guest_mb << 20)) rt_fatal("guest memory: allocation failed");
    rt_log("[mem] guest memory: %u MB", guest_mb);
    char exe[600];
    snprintf(exe, sizeof exe, "%s/SILENT HILL 4.exe", g_game_dir);
    int r = rt_load_exe(exe);
    if (r) rt_fatal("cannot load %s (%d)", exe, r);
    rt_fill_iat();
    mods_init();
    rt_log("game loaded from %s", g_game_dir);
    rt_run_entry();
    return 0;
}

#ifdef _WIN32
int main(int argc, char **argv);

/* A Windows program, not a console: the release has no window behind the launcher and no sh4.log on stdout. */
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)inst, (void)prev, (void)show;
    return main(__argc, __argv);
}
#endif

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    /* A path can come from the command line, SH4_DIR, or a shortcut. It is checked, never guessed at: a path that
     * is not this game says so instead of quietly using another folder. */
    const char *given = argc > 1 && argv[1] && argv[1][0] ? argv[1] : getenv("SH4_DIR");
    if (given && *given) {
        char dir[512], msg[1200];
        int kind = sh4_check_copy(given, dir, sizeof dir, msg, sizeof msg);
        if (kind == SH4_COPY_GAME) snprintf(g_game_dir, sizeof g_game_dir, "%s", dir);
        else rt_log("argument: %s", msg);
    }

    if (!getenv("SH4_SKIP_LAUNCHER")) launcher_run();
    if (!g_game_dir[0]) {
        rt_log("no game folder: pass the Silent Hill 4 folder as the first argument, set SH4_DIR, or pick it in "
               "Options > Game folder");
        SDL_Quit();
        return 1;
    }
    int rc = load_and_run(g_game_dir);
    SDL_Quit();
    return rc;
}
