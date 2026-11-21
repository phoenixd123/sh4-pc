/* F12: the frame that was just drawn, at the game's own resolution, as a PNG in screenshots/ beside the game.
 * Called from the present path (d3d8/draw.c) before the buffer swap, so it captures exactly what was presented. */
#include "rt.h"
#include "../fs.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../mods/stb_image_write.h"   /* the implementation is compiled once, in src/mods/stb_impl.c */

static int wanted;

void sh4_shot_request(void) { wanted = 1; }

void plat_frame_presented(void) {
    if (!wanted) return;
    wanted = 0;
    int w = 0, h = 0;
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    if (!win) return;
    SDL_GL_GetDrawableSize(win, &w, &h);
    if (w <= 0 || h <= 0) return;
    size_t row = (size_t)w * 4;
    uint8_t *px = malloc(row * (size_t)h);
    if (!px) return;
    /* gfx_present has already bound the default framebuffer for the final blit. */
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (int y = 0; y < h / 2; y++)
        for (size_t x = 0; x < row; x++) {
            uint8_t t = px[y * row + x];
            px[y * row + x] = px[(size_t)(h - 1 - y) * row + x];
            px[(size_t)(h - 1 - y) * row + x] = t;
        }
    for (size_t i = 3; i < row * (size_t)h; i += 4) px[i] = 255;  /* the back buffer has no alpha */

    mkdir("screenshots", 0755);
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char path[128];
    snprintf(path, sizeof path, "screenshots/sh4-%04d%02d%02d-%02d%02d%02d.png", tm->tm_year + 1900, tm->tm_mon + 1,
             tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    if (stbi_write_png(path, w, h, 4, px, (int)row)) rt_log("[shot] %s (%dx%d)", path, w, h);
    else rt_log("[shot-error] %s could not be written", path);
    free(px);
}
