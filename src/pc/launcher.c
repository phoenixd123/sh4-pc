/* Before the game: a warning screen (assets/splash.bmp), then Play, Options and Credits. Everything is drawn on the
 * CPU into a 1280x720 canvas and scaled into the window, so the launcher needs no OpenGL state of its own and the
 * game gets a fresh context when Play is chosen.
 *
 * The images are built into the binary as BMPs (CMake passes SRC_DIR; tools/launcher_assets.py draws them) and SDL
 * loads them itself. */
#include "launcher.h"
#include "../rt/rt.h"
#include "../mods/mods.h"
#include "copy_check.h"
#ifdef _WIN32
#include "nexus.h"
#include "updater.h"
#endif
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef _WIN32
#include <strings.h>
#include <unistd.h>
#else
#include <direct.h>
#define chdir _chdir
#endif

#ifndef PORT_AUTHOR
#define PORT_AUTHOR "phoenixd123"
#endif
#ifndef PORT_NAME
#define PORT_NAME "SH4-PC"
#endif

#define FADE_MS 600
#define CW 1280
#define CH 720

/* .incbin assets: PE-COFF has no .previous, so the Windows form names the sections explicitly. */
#ifdef _WIN32
#define ASSET(name, path) \
    __asm__(".section .rdata,\"dr\"\n.balign 16\n.globl " #name "_bmp\n" #name "_bmp:\n.incbin \"" SRC_DIR "/" path "\"\n" \
            ".globl " #name "_bmp_end\n" #name "_bmp_end:\n.text"); \
    extern const uint8_t name##_bmp[], name##_bmp_end[];
#else
#define ASSET(name, path) \
    __asm__(".section .rodata\n.balign 16\n.global " #name "_bmp\n" #name "_bmp:\n.incbin \"" SRC_DIR "/" path "\"\n" \
            ".global " #name "_bmp_end\n" #name "_bmp_end:\n.previous"); \
    extern const uint8_t name##_bmp[], name##_bmp_end[];
#endif
ASSET(splash, "assets/splash.bmp")
ASSET(logo, "assets/logo.bmp")
ASSET(font, "assets/font.bmp")  /* ASCII 32-127, 16x6 cells of 40x80, baseline at 60, 36 px advance */

Sh4Settings g_settings = {"1280x720", 0, 1, 0, 8, ""};

static void mods_screen(void);  /* the Mods screen, further down: Options and the menu both open it */

/* ---- settings: sh4.ini ---- */

void sh4_settings_load(void) {
    FILE *f = fopen("sh4.ini", "r");
    char line[256], k[64], v[128];
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || !*p) continue;
        if (sscanf(p, " %63[^= \t] = %127[^ \t\r\n;]", k, v) != 2) continue;
        if (!strcasecmp(k, "resolution")) snprintf(g_settings.resolution, sizeof g_settings.resolution, "%s", v);
        else if (!strcasecmp(k, "fullscreen")) g_settings.fullscreen = atoi(v) != 0;
        else if (!strcasecmp(k, "vsync")) g_settings.vsync = atoi(v) != 0;
        else if (!strcasecmp(k, "filter")) g_settings.filter = atoi(v) > 2 ? 2 : atoi(v) < 0 ? 0 : atoi(v);
        else if (!strcasecmp(k, "aniso")) {
            g_settings.aniso = atoi(v);
            if (g_settings.aniso < 1) g_settings.aniso = 1;
            if (g_settings.aniso > 16) g_settings.aniso = 16;
        } else if (!strcasecmp(k, "nexus_key")) snprintf(g_settings.nexus_key, sizeof g_settings.nexus_key, "%s", v);
    }
    fclose(f);
}

void sh4_settings_save(void) {
    char *lines[256];
    int n = 0;
    struct { const char *key, *val; } want[] = {
        {"resolution", g_settings.resolution},
        {"fullscreen", g_settings.fullscreen ? "1" : "0"},
        {"vsync", g_settings.vsync ? "1" : "0"},
        {"filter", g_settings.filter == 1 ? "1" : g_settings.filter == 2 ? "2" : "0"},
        {"aniso", g_settings.aniso == 16 ? "16" : g_settings.aniso == 8 ? "8" :
                  g_settings.aniso == 4 ? "4" : g_settings.aniso == 2 ? "2" : "1"},
        {"nexus_key", g_settings.nexus_key},
    };
    const int nwant = (int)(sizeof want / sizeof *want);
    FILE *f = fopen("sh4.ini", "r");
    char line[256];
    while (f && n < 256 && fgets(line, sizeof line, f)) lines[n++] = strdup(line);
    if (f) fclose(f);
    if (!(f = fopen("sh4.ini", "w"))) {
        rt_log("launcher: cannot write sh4.ini");
        return;
    }
    for (int w = 0; w < nwant; w++) {
        int last = -1;
        char key[64], val[128];
        for (int i = 0; i < n; i++)
            if (sscanf(lines[i], " %63[^= \t] = %127[^ \t\r\n;]", key, val) == 2 && !strcasecmp(key, want[w].key))
                last = i;
        char set[160];
        snprintf(set, sizeof set, "%s=%s\n", want[w].key, want[w].val);
        if (last >= 0) {
            free(lines[last]);
            lines[last] = strdup(set);
        } else
            lines[n++] = strdup(set);
    }
    for (int i = 0; i < n; i++) fputs(lines[i], f), free(lines[i]);
    fclose(f);
}

/* ---- the game folder, remembered in sh4-pc.ini beside the executable ---- */

static void game_ini(char *out, size_t n, int write) {
    FILE *f = fopen("sh4-pc.ini", write ? "w" : "r");
    if (out && n) out[0] = 0;
    if (!f) return;
    if (write) {
        fprintf(f, "game=%s\n", g_game_dir);
    } else {
        char line[600];
        while (fgets(line, sizeof line, f)) {
            char v[512];
            if (sscanf(line, " game = %511[^\r\n]", v) == 1) {
                size_t k = strlen(v);
                while (k && (v[k - 1] == ' ' || v[k - 1] == '\t')) v[--k] = 0;
                snprintf(out, n, "%s", v);
                break;
            }
        }
    }
    fclose(f);
}

/* ---- images ---- */

static uint32_t *decode(const uint8_t *bmp, const uint8_t *end, int *w, int *h) {
    SDL_Surface *s = SDL_LoadBMP_RW(SDL_RWFromConstMem(bmp, (int)(end - bmp)), 1), *c;
    uint32_t *px = NULL;
    if (s && (c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0))) {
        *w = c->w;
        *h = c->h;
        if ((px = malloc((size_t)*w * *h * 4)))
            for (int y = 0; y < *h; y++)
                memcpy(px + (size_t)y * *w, (uint8_t *)c->pixels + (size_t)y * c->pitch, (size_t)*w * 4);
        SDL_FreeSurface(c);
    }
    SDL_FreeSurface(s);
    return px;
}

/* ---- the canvas ---- */

static uint32_t canvas[CW * CH];
static uint32_t *layer = canvas;
static int glyph_track;
static uint32_t *font_px;
static int font_w;
static SDL_Window *win;
static SDL_Surface *screen;

static uint32_t rgb(int r, int g, int b) { return 0xFF000000u | (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r; }

static void present(void) {
    if (!win) return;
    if (!screen) screen = SDL_GetWindowSurface(win);
    if (!screen) return;
    SDL_Rect dst;
    float s = fminf((float)screen->w / CW, (float)screen->h / CH);
    dst.w = (int)(CW * s), dst.h = (int)(CH * s);
    dst.x = (screen->w - dst.w) / 2, dst.y = (screen->h - dst.h) / 2;
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
    SDL_Surface *src = SDL_CreateRGBSurfaceWithFormatFrom(canvas, CW, CH, 32, CW * 4, SDL_PIXELFORMAT_ARGB8888);
    if (src) {
        SDL_BlitScaled(src, NULL, screen, &dst);
        SDL_FreeSurface(src);
    }
    SDL_UpdateWindowSurface(win);
}

/* Text at x (left edge) and baseline y; size = glyph height in px (80 = the atlas), sampled bilinearly. */
static int text(int x, int y, int size, uint32_t color, const char *s) {
    float k = size / 80.0f;
    int adv = (int)(36 * k + 0.5f), cw = (int)(40 * k + 0.5f), ch = size, top = y - (int)(60 * k);
    for (; *s; s++, x += adv + glyph_track) {
        int c = (unsigned char)*s - 32;
        if (c <= 0 || c >= 96 || !font_px) continue;
        int gx = c % 16 * 40, gy = c / 16 * 80;
        for (int py = 0; py < ch; py++) {
            int oy = top + py;
            if (oy < 0 || oy >= CH) continue;
            float fy = (py + 0.5f) / k - 0.5f;
            int y0 = (int)fy;
            float ty = fy - y0;
            if (y0 < 0) y0 = 0, ty = 0;
            if (y0 >= 79) y0 = 78, ty = 1;
            for (int px = 0; px < cw; px++) {
                int ox = x + px;
                if (ox < 0 || ox >= CW) continue;
                float fx = (px + 0.5f) / k - 0.5f;
                int x0 = (int)fx;
                float tx = fx - x0;
                if (x0 < 0) x0 = 0, tx = 0;
                if (x0 >= 39) x0 = 38, tx = 1;
                const uint32_t *r0 = font_px + (size_t)(gy + y0) * font_w + gx + x0, *r1 = r0 + font_w;
                float a = ((r0[0] >> 24) * (1 - tx) + (r0[1] >> 24) * tx) * (1 - ty) +
                          ((r1[0] >> 24) * (1 - tx) + (r1[1] >> 24) * tx) * ty;
                if (a < 1) continue;
                uint32_t *d = &layer[oy * CW + ox], o = 0xFF000000u;
                for (int sh = 0; sh < 24; sh += 8) {
                    float v = ((*d >> sh) & 0xFF) + (((color >> sh) & 0xFF) - (float)((*d >> sh) & 0xFF)) * a / 255;
                    o |= (uint32_t)v << sh;
                }
                *d = o;
            }
        }
    }
    return x;
}

/* An RGBA image scaled bilinearly into the canvas rect x, y, dw, dh, blended by its alpha. */
static void image(const uint32_t *img, int w, int h, int x, int y, int dw, int dh) {
    for (int py = 0; py < dh; py++) {
        float fy = (py + 0.5f) * h / dh - 0.5f;
        int y0 = fy < 0 ? 0 : (int)fy, y1 = y0 + 1 < h ? y0 + 1 : y0;
        float ty = fy < 0 ? 0 : fy - y0;
        for (int px = 0; px < dw; px++) {
            float fx = (px + 0.5f) * w / dw - 0.5f;
            int x0 = fx < 0 ? 0 : (int)fx, x1 = x0 + 1 < w ? x0 + 1 : x0;
            float tx = fx < 0 ? 0 : fx - x0;
            const uint32_t q[4] = {img[y0 * w + x0], img[y0 * w + x1], img[y1 * w + x0], img[y1 * w + x1]};
            float k[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty}, a = 0, c[3] = {0};
            for (int i = 0; i < 4; i++) {
                float qa = (q[i] >> 24) * k[i];
                a += qa;
                for (int j = 0; j < 3; j++) c[j] += ((q[i] >> 8 * j) & 0xFF) * qa;
            }
            if (a < 1) continue;
            uint32_t *d = &layer[(y + py) * CW + x + px], o = 0xFF000000u;
            for (int j = 0; j < 3; j++) {
                float dv = (*d >> 8 * j) & 0xFF;
                o |= (uint32_t)(dv + (c[j] / a - dv) * a / 255) << 8 * j;
            }
            *d = o;
        }
    }
}

static int text_w(int size, const char *s) { return (int)strlen(s) * (int)(36 * size / 80.0f + 0.5f); }
static void text_c(int y, int size, uint32_t color, const char *s) { text((CW - text_w(size, s)) / 2, y, size, color, s); }
static void text_ct(int y, int size, uint32_t color, const char *s, int track) {
    int n = (int)strlen(s), adv = (int)(36 * size / 80.0f + 0.5f);
    glyph_track = track;
    text((CW - (n * (adv + track) - track)) / 2, y, size, color, s);
    glyph_track = 0;
}

/* ---- input ---- */

enum { IN_NONE, IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_OK, IN_BACK, IN_DROP, IN_REFRESH, IN_PAGE, IN_MODS };

static char drop_file[700];

/* Testing: SH4_KEYS drives the menus with "time:Key[,time:Key...]" (the same shape the port's scripts use). */
static int scripted_menu_key(void) {
    static Uint64 t0;
    static double fired[64];
    static int nfired;
    const char *k = getenv("SH4_KEYS");
    if (!k) return IN_NONE;
    if (!t0) t0 = SDL_GetTicks64();
    double now = (SDL_GetTicks64() - t0) / 1000.0;
    while (*k && nfired < (int)(sizeof fired / sizeof *fired)) {
        char name[32] = "";
        double at = 0, hold = 0.15;
        int n = 0, code = IN_NONE, seen = 0;
        if (sscanf(k, "%lf:%31[^,+]%n", &at, name, &n) < 2) break;
        k += n;
        if (*k == '+' && sscanf(k, "+%lf%n", &hold, &n) == 1) k += n;
        if (!strcasecmp(name, "Up")) code = IN_UP;
        else if (!strcasecmp(name, "Down")) code = IN_DOWN;
        else if (!strcasecmp(name, "Left")) code = IN_LEFT;
        else if (!strcasecmp(name, "Right")) code = IN_RIGHT;
        else if (!strcasecmp(name, "Return") || !strcasecmp(name, "Space")) code = IN_OK;
        else if (!strcasecmp(name, "Escape")) code = IN_BACK;
        for (int i = 0; i < nfired; i++)
            if (fired[i] == at) seen = 1;
        if (code && !seen && now >= at && now < at + hold) {
            fired[nfired++] = at;
            rt_log("launcher: scripted %s at %.2f\n", name, at);
            return code;
        }
        if (*k == ',') k++;
    }
    return IN_NONE;
}

static int poll_input(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: exit(0);
        case SDL_DROPFILE:
            snprintf(drop_file, sizeof drop_file, "%s", e.drop.file);
            SDL_free(e.drop.file);
            return IN_DROP;
        case SDL_CONTROLLERDEVICEADDED: SDL_GameControllerOpen(e.cdevice.which); break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym) {
            case SDLK_UP: return IN_UP;
            case SDLK_DOWN: return IN_DOWN;
            case SDLK_LEFT: return IN_LEFT;
            case SDLK_RIGHT: return IN_RIGHT;
            case SDLK_RETURN: case SDLK_SPACE: return IN_OK;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: return IN_BACK;
            case SDLK_m: return IN_MODS;
            case SDLK_r: return IN_REFRESH;
            case SDLK_n: return IN_PAGE;
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP: return IN_UP;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return IN_DOWN;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return IN_LEFT;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return IN_RIGHT;
            case SDL_CONTROLLER_BUTTON_A: return IN_OK;
            case SDL_CONTROLLER_BUTTON_B: return IN_BACK;
            case SDL_CONTROLLER_BUTTON_START: return IN_OK;
            }
            break;
        case SDL_CONTROLLERAXISMOTION: {
            static int held[2];
            int ax = e.caxis.axis;
            if (ax != SDL_CONTROLLER_AXIS_LEFTX && ax != SDL_CONTROLLER_AXIS_LEFTY) break;
            int dir = e.caxis.value > 16000 ? 1 : e.caxis.value < -16000 ? -1 : 0, was = held[ax];
            held[ax] = dir;
            if (dir && dir != was)
                return ax == SDL_CONTROLLER_AXIS_LEFTX ? (dir > 0 ? IN_RIGHT : IN_LEFT) : (dir > 0 ? IN_DOWN : IN_UP);
            break;
        }
        }
    }
    return scripted_menu_key();
}

/* ---- palette (the canvas is ARGB8888) ---- */
#define C_BG 0xFF0A0808u
#define C_TEXT 0xFFC8C8C8u
#define C_DIM 0xFF6E6E6Eu
#define C_SEL 0xFF1E28DCu  /* a muted rust, nearer this game than the last one's red */
#define C_RULE 0xFF4A4646u

static uint32_t *logo_px;
static int logo_w, logo_h;

static void fade(int in) {
    static uint32_t pic[CW * CH];
    memcpy(pic, canvas, sizeof pic);
    for (Uint64 t0 = SDL_GetTicks64(), t = 0; t < FADE_MS;) {
        poll_input();
        t = SDL_GetTicks64() - t0;
        uint32_t k = t >= FADE_MS ? 256 : (uint32_t)(t * 256 / FADE_MS);
        if (!in) k = 256 - k;
        for (int i = 0; i < CW * CH; i++)
            canvas[i] = 0xFF000000u | ((pic[i] & 0xFF00FFu) * k >> 8 & 0xFF00FFu) | ((pic[i] & 0xFF00u) * k >> 8 & 0xFF00u);
        present();
    }
    memcpy(canvas, pic, sizeof pic);
}

/* ---- credits ---- */

enum { CR_TITLE, CR_RULE, CR_GAP, CR_LABEL, CR_VALUE, CR_NOTE };
typedef struct { int kind; const char *text; } Credit;

static const Credit credit_roll[] = {
    {CR_TITLE, "SILENT HILL 4"},
    {CR_RULE, NULL},
    {CR_GAP, NULL},
    {CR_LABEL, "PORTED TO PC BY"},
    {CR_VALUE, PORT_AUTHOR},
    {CR_GAP, NULL},
    {CR_LABEL, "ORIGINAL GAME"},
    {CR_VALUE, "Konami / Team Silent"},
    {CR_GAP, NULL},
    {CR_LABEL, "THE PORT THIS STANDS ON"},
    {CR_VALUE, "sh4vita by vallfrr"},
    {CR_GAP, NULL},
    {CR_LABEL, "EVERYTHING ELSE"},
    {CR_VALUE, "CREDITS.md"},
    {CR_GAP, NULL},
    {CR_GAP, NULL},
    {CR_NOTE, "Unofficial fan port. No game files are included:"},
    {CR_NOTE, "it runs the copy of Silent Hill 4: The Room that you own."},
};
#define N_CREDITS (int)(sizeof credit_roll / sizeof *credit_roll)

static int credit_h(int kind) {
    switch (kind) {
    case CR_TITLE: return 46 + 22;
    case CR_RULE: return 1 + 26;
    case CR_GAP: return 30;
    case CR_LABEL: return 18 + 14;
    case CR_VALUE: return 30 + 30;
    default: return 17 + 10;
    }
}

static void rule(int y, int w) {
    if (y < 0 || y >= CH) return;
    for (int x = (CW - w) / 2; x < (CW + w) / 2; x++) layer[y * CW + x] = C_RULE;
}

static void credit(const Credit *c, int *y, int draw) {
    switch (c->kind) {
    case CR_TITLE: if (draw) text_ct(*y, 46, C_TEXT, c->text, 12); break;
    case CR_RULE: if (draw) rule(*y + 8, 300); break;
    case CR_LABEL: if (draw) text_ct(*y, 18, C_DIM, c->text, 7); break;
    case CR_VALUE: if (draw) text_ct(*y, 30, C_TEXT, c->text, 2); break;
    case CR_NOTE: if (draw) text_ct(*y, 17, C_DIM, c->text, 1); break;
    }
    *y += credit_h(c->kind);
}

static int credits_height(void) {
    int y = 0;
    for (int i = 0; i < N_CREDITS; i++) credit(&credit_roll[i], &y, 0);
    return y;
}

static void draw_credits(int top) {
    for (int i = 0; i < CW * CH; i++) canvas[i] = C_BG;
    layer = canvas;
    int y = top;
    for (int i = 0; i < N_CREDITS; i++) credit(&credit_roll[i], &y, 1);
    for (int r = 0; r < CH; r++) {
        int a = r * 255 / 130, b = (CH - 1 - r) * 255 / 190, v = a < b ? a : b;
        uint32_t k = (uint32_t)(v > 255 ? 255 : v);
        if (k == 255) continue;
        for (int x = 0; x < CW; x++) {
            uint32_t *d = &canvas[r * CW + x];
            *d = 0xFF000000u | ((*d & 0xFF00FFu) * k >> 8 & 0xFF00FFu) | ((*d & 0xFF00u) * k >> 8 & 0xFF00u);
        }
    }
}

static void credits(void) {
    const int height = credits_height(), speed = 60;
    Uint64 t0 = SDL_GetTicks64();
    draw_credits(CH + 20);
    fade(1);
    for (;;) {
        int top = CH + 20 - (int)((SDL_GetTicks64() - t0) * (Uint64)speed / 1000);
        if (top < -height - 20) break;
        draw_credits(top);
        present();
        int in = poll_input();
        if (in == IN_OK || in == IN_BACK) break;
        SDL_Delay(8);
    }
}

/* ---- Options ---- */

typedef struct {
    const char *label, *key;
    const char *vals[6], *names[6];
    int cur, was;
} Opt;

static Opt opts[] = {
    {"Resolution", "resolution", {"1280x720", "1920x1080", "1366x768", "1024x768", "640x480"},
                               {"720p", "1080p", "1366x768", "1024x768", "640x480"}},
    {"Fullscreen", "fullscreen", {"0", "1"}, {"Off", "On"}},
    {"Vsync", "vsync", {"1", "0"}, {"On", "Off"}},
    {"Filter", "filter", {"0", "1", "2"}, {"Off", "CRT", "VHS"}},
    {"Anisotropic filtering", "aniso", {"1", "2", "4", "8", "16"}, {"Off", "2x", "4x", "8x", "16x"}},
};
#define N_OPTS (int)(sizeof opts / sizeof *opts)

#define ROW_FOLDER N_OPTS
#define ROW_DISC (N_OPTS + 1)
#define ROW_CHECK (N_OPTS + 2)
#define ROW_IMPORT (N_OPTS + 3)
#define ROW_MOD (N_OPTS + 4)
#define ROW_UPDATE (N_OPTS + 5)
#define ROW_BACK (N_OPTS + 6)
#define N_ROWS (ROW_BACK + 1)

static int n_vals(const Opt *o) { int n = 0; while (n < 6 && o->vals[n]) n++; return n; }
static const char *opt_name(const Opt *o) { return o->names[o->cur]; }

static void options_load(void) {
    for (int i = 0; i < N_OPTS; i++)
        for (int j = 0; j < n_vals(&opts[i]); j++) opts[i].cur = j;
    if (g_settings.resolution[0])
        for (int j = 0; j < n_vals(&opts[0]); j++)
            if (!strcasecmp(opts[0].vals[j], g_settings.resolution)) opts[0].cur = j;
    opts[1].cur = g_settings.fullscreen ? 1 : 0;
    opts[2].cur = g_settings.vsync ? 0 : 1;
    opts[3].cur = g_settings.filter == 0 ? 0 : g_settings.filter == 1 ? 1 : 2;
    for (int j = 0; j < n_vals(&opts[4]); j++)
        if (atoi(opts[4].vals[j]) == g_settings.aniso) opts[4].cur = j;
    for (int i = 0; i < N_OPTS; i++) opts[i].was = opts[i].cur;
}

static void options_apply(void) {
    snprintf(g_settings.resolution, sizeof g_settings.resolution, "%s", opts[0].vals[opts[0].cur]);
    g_settings.fullscreen = opts[1].cur == 1;
    g_settings.vsync = opts[2].cur == 0;
    g_settings.filter = atoi(opts[3].vals[opts[3].cur]);
    g_settings.aniso = atoi(opts[4].vals[opts[4].cur]);
}

static const char *row_note;

static void draw_options(int sel) {
    for (int i = 0; i < CW * CH; i++) canvas[i] = C_BG;
    glyph_track = 0;
    text_c(110, 56, C_TEXT, "Options");
    for (int i = 0; i < N_ROWS; i++) {
        int y = 160 + i * 44;
        uint32_t c = i == sel ? C_SEL : C_TEXT;
        if (i == ROW_BACK) { text_c(y, 30, c, "Back"); continue; }
        if (i == ROW_FOLDER) {
            text(140, y, 30, c, "Game folder");
            const char *p = g_game_dir[0] ? g_game_dir : "(none yet)";
            int maxw = CW - 300;
            if (text_w(30, p) > maxw) {
                static char buf[600];
                int n = (int)strlen(p), keep = n;
                while (keep > 0 && text_w(30, p + n - keep) + text_w(30, "...") > maxw) keep--;
                snprintf(buf, sizeof buf, "...%s", p + n - keep);
                p = buf;
            }
            text(CW - 140 - text_w(30, p), y, 30, i == sel ? C_SEL : C_DIM, p);
            continue;
        }
        if (i == ROW_DISC) { text(140, y, 30, c, "Game file or disc"); continue; }
        if (i == ROW_CHECK) { text(140, y, 30, c, "Check this copy"); continue; }
        if (i == ROW_IMPORT) { text(140, y, 30, c, "Import a .zip download"); continue; }
        if (i == ROW_MOD) { text(140, y, 30, c, "Install a mod"); continue; }
        if (i == ROW_UPDATE) { text(140, y, 30, c, "Check for updates"); continue; }
        text(140, y, 30, c, opts[i].label);
        char v[64];
        snprintf(v, sizeof v, i == sel ? "< %s >" : "%s", opt_name(&opts[i]));
        text(CW - 140 - text_w(26, v), y, 26, c, v);
    }
    text_c(662, 20, C_DIM, "Left/Right: change    A: pick    B: back (saves sh4.ini)");
    if (row_note) text_c(692, 20, C_DIM, row_note);
}

/* ---- the rows' work ---- */

static void say(const char *title, const char *text) {
#ifdef _WIN32
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, text, win);
#else
    rt_log("%s: %s", title, text);
#endif
}

static void use_game_dir(const char *dir) {
    snprintf(g_game_dir, sizeof g_game_dir, "%s", dir);
    game_ini(NULL, 0, 1);
    if (chdir(g_game_dir)) rt_log("launcher: cannot enter %s", g_game_dir);
    mods_init();
    sh4_settings_load();
    options_load();
    row_note = "Game folder set";
}

static void install_mod(const char *zip) {
    char name[128], err[700] = "";
    if (!g_game_dir[0]) {
        say("Install a mod", "Pick the game folder first: a mod is installed into mods/ beside the game.");
        return;
    }
    if (mods_install(zip, name, sizeof name, err, sizeof err)) {
        static char note[200];
        mods_init();
        snprintf(note, sizeof note, "Installed the mod \"%s\"", name);
        row_note = note;
    } else
        say("Install a mod", err);
}

static void import_zip(const char *zip) {
    char dir[700], msg[1200];
    if (sh4_import_zip(zip, dir, sizeof dir, msg, sizeof msg)) {
        use_game_dir(dir);
        row_note = "Imported the download";
        say("Import a .zip download", msg);
    } else
        say("Import a .zip download", msg);
}

void launcher_drop(const char *path) {
    char dir[512], msg[1200];
    int kind = sh4_check_copy(path, dir, sizeof dir, msg, sizeof msg);
    if (kind == SH4_COPY_GAME) {
        use_game_dir(dir);
        say("Silent Hill 4", msg);
    } else if (kind == SH4_COPY_DOWNLOAD) {
        import_zip(path);
    } else if (kind == SH4_COPY_MOD) {
        install_mod(path);
    } else if (kind == SH4_COPY_PS2) {
        if (sh4_open_ps2(path)) row_note = "Opened in PCSX2";
        else say("Silent Hill 4", msg);
    } else
        say("Silent Hill 4", msg);
}

#ifdef _WIN32
/* Options > Check for updates. The launcher replaces its own files once it exits; the script the updater writes
 * waits for this process and starts it again. */
static void check_updates(void) {
    UpdaterRelease r;
    char err[700] = "";
    int n = updater_check(&r, err, sizeof err);
    if (n == 0) {
        say("Updates", "This is the newest version of the launcher.");
        row_note = "Up to date";
        return;
    }
    if (n < 0) {
        say("Updates", err);
        row_note = "Could not check for updates";
        return;
    }
    char q[900];
    snprintf(q, sizeof q, "Version %s is available (this build is %s).\n\nDownload it and restart the launcher?",
             r.version, updater_version());
    if (!sh4_confirm("Update", q)) {
        row_note = "Update available";
        return;
    }
    char zip[700];
    if (!updater_download(r.url, zip, sizeof zip, err, sizeof err)) {
        say("Updates", err);
        return;
    }
    if (!updater_install(zip, err, sizeof err)) {
        say("Updates", err);
        return;
    }
    say("Updates", "The update is applied now that the launcher is closing; it starts again by itself.");
    exit(0);
}
#endif

static void options(void) {
    int sel = 0;
    row_note = NULL;
    draw_options(sel);
    fade(1);
    for (int dirty = 0;;) {
        if (dirty) { draw_options(sel); dirty = 0; }
        present();
        int in = poll_input();
        if (in == IN_DROP) {
            launcher_drop(drop_file);
            draw_options(sel);
            continue;
        }
        if (in == IN_MODS) {
            mods_screen();
            draw_options(sel);
            continue;
        }
        if (in == IN_NONE) continue;
        dirty = 1;
        if (in == IN_UP) sel = (sel + N_ROWS - 1) % N_ROWS;
        if (in == IN_DOWN) sel = (sel + 1) % N_ROWS;
        if (sel < N_OPTS && (in == IN_LEFT || in == IN_RIGHT || in == IN_OK)) {
            Opt *o = &opts[sel];
            int n = n_vals(o);
            o->cur = (o->cur + (in == IN_LEFT ? n - 1 : 1)) % n;
            options_apply();
        }
        if (sel == ROW_FOLDER && (in == IN_LEFT || in == IN_RIGHT || in == IN_OK)) {
            char picked[700], dir[512], msg[1200];
            if (sh4_pick_folder(g_game_dir, picked, sizeof picked)) {
                int kind = sh4_check_copy(picked, dir, sizeof dir, msg, sizeof msg);
                if (kind == SH4_COPY_GAME) {
                    use_game_dir(dir);
                    say("Silent Hill 4", msg);
                } else {
                    row_note = kind == SH4_COPY_WRONG_EXE ? "Wrong executable: see the message"
                                                          : "That folder is not a playable copy";
                    say("Silent Hill 4", msg);
                }
            }
        }
        if (sel == ROW_DISC && (in == IN_LEFT || in == IN_RIGHT || in == IN_OK)) {
            char picked[700], dir[512], msg[1200];
            if (sh4_pick_game_file(g_game_dir, picked, sizeof picked)) {
                int kind = sh4_check_copy(picked, dir, sizeof dir, msg, sizeof msg);
                if (kind == SH4_COPY_GAME) {
                    use_game_dir(dir);
                    say("Silent Hill 4", msg);
                } else if (kind == SH4_COPY_PS2) {
                    if (sh4_open_ps2(picked)) row_note = "Opened in PCSX2";
                    else {
                        row_note = sh4_pcsx2_found() ? "PlayStation 2 disc: not opened"
                                                     : "PlayStation 2 disc: PCSX2 is not installed";
                        say("Silent Hill 4", msg);
                    }
                } else {
                    row_note = "Not the PC release";
                    say("Silent Hill 4", msg);
                }
            }
        }
        if (sel == ROW_CHECK && (in == IN_LEFT || in == IN_RIGHT || in == IN_OK)) {
            char picked[700], dir[512], msg[1200];
            if (sh4_pick_folder(g_game_dir, picked, sizeof picked)) {
                int kind = sh4_check_copy(picked, dir, sizeof dir, msg, sizeof msg);
                row_note = kind == SH4_COPY_GAME ? "Playable copy"
                          : kind == SH4_COPY_DOWNLOAD ? "Game download"
                          : kind == SH4_COPY_WRONG_EXE ? "Not the supported executable"
                          : kind == SH4_COPY_PS2 ? "PlayStation 2 disc"
                          : kind == SH4_COPY_MOD ? "Mod archive"
                                                  : "Not a Silent Hill 4 folder";
                if (kind == SH4_COPY_GAME) {
                    if (sh4_confirm("Use this copy?", msg)) use_game_dir(dir);
                } else if (kind == SH4_COPY_DOWNLOAD) {
                    if (sh4_confirm("Import this download?", msg)) import_zip(picked);
                } else if (kind == SH4_COPY_PS2) {
                    if (sh4_open_ps2(picked)) row_note = "Opened in PCSX2";
                    else say("Silent Hill 4", msg);
                } else
                    say("Silent Hill 4", msg);
            }
        }
        if (sel == ROW_IMPORT && (in == IN_LEFT || in == IN_RIGHT || in == IN_OK)) {
            char zip[700];
            if (sh4_pick_game_archive(g_game_dir, zip, sizeof zip)) import_zip(zip);
        }
        if (sel == ROW_MOD && (in == IN_LEFT || in == IN_RIGHT || in == IN_OK)) {
            char zip[700];
            if (sh4_pick_archive(g_game_dir, zip, sizeof zip)) install_mod(zip);
        }
        if (sel == ROW_UPDATE && (in == IN_LEFT || in == IN_RIGHT || in == IN_OK)) {
#ifdef _WIN32
            check_updates();
#else
            row_note = "Updates are a Windows launcher feature";
#endif
        }
        if (in == IN_BACK || (sel == ROW_BACK && in == IN_OK)) {
            sh4_settings_save();
            fade(0);
            return;
        }
    }
}

/* ---- Mods: installed, and the Nexus list ------------------------------------------------ */

#ifdef _WIN32
static NexusMod nx_mods[NEXUS_MAX_MODS];
static int nx_count;
static volatile int nx_list_done;
static char nx_list_err[600];
static char nx_note[600];

typedef struct {
    int mod;
    char zip[700];
    char err[600];
    int ok;
} NxJob;
static NxJob nx_job;
static volatile int nx_job_done;

static int SDLCALL nx_list_thread(void *unused) {
    char err[600] = "";
    (void)unused;
    int n = nexus_list(nx_mods, NEXUS_MAX_MODS, err, sizeof err);
    if (n > 0) nx_count = n;
    else snprintf(nx_list_err, sizeof nx_list_err, "%s", err);
    nx_list_done = 1;
    return 0;
}

static int SDLCALL nx_dl_thread(void *unused) {
    char err[600] = "", who[64] = "";
    (void)unused;
    int prem = nexus_account(who, sizeof who, err, sizeof err);
    rt_log("mods: account %d %s", prem, who);
    if (prem != 1) {
        if (prem == 0)
            snprintf(err, sizeof err,
                     "Automatic downloads need Nexus Premium. Press N to open the mod's page, download the file "
                     "there, and drop the .zip on this window to install it.");
        snprintf(nx_job.err, sizeof nx_job.err, "%s", err);
        nx_job_done = 1;
        return 0;
    }
    nx_job.ok = nexus_fetch_main(nx_job.mod, nx_job.zip, err, sizeof err);
    rt_log("mods: fetch %d -> %d %s", nx_job.mod, nx_job.ok, err);
    if (!nx_job.ok) snprintf(nx_job.err, sizeof nx_job.err, "%s", err);
    nx_job_done = 1;
    return 0;
}

/* Long sentences over as many lines as they need. */
static void wrap_text(int x, int y, int size, uint32_t c, const char *s) {
    char line[300] = "";
    size_t ll = 0;
    int maxw = CW - 2 * x - 20;
    while (*s) {
        if (*s == '\n') {
            if (ll) text(x, y, size, c, line);
            y += size + 8;
            line[0] = 0;
            ll = 0;
            s++;
            continue;
        }
        const char *w = s;
        while (*w && *w != ' ' && *w != '\n') w++;
        char cand[320];
        snprintf(cand, sizeof cand, "%s%s%.*s", line, ll ? " " : "", (int)(w - s), s);
        if (text_w(size, cand) > maxw && ll) {
            text(x, y, size, c, line);
            y += size + 8;
            snprintf(line, sizeof line, "%.*s", (int)(w - s), s);
            ll = strlen(line);
        } else {
            snprintf(line, sizeof line, "%s", cand);
            ll = strlen(line);
        }
        s = *w ? w + 1 : w;
    }
    if (ll) text(x, y, size, c, line);
}

static void draw_mods(int sel) {
    for (int i = 0; i < CW * CH; i++) canvas[i] = C_BG;
    text_c(24, 34, C_TEXT, "MODS");
    text_c(76, 17, C_DIM, "installed from mods/");
    int n = mods_count();
    if (!n) text(70, 110, 20, C_DIM, "None yet.");
    for (int i = 0; i < n && i < 14; i++) text(70, 112 + i * 30, 20, C_TEXT, mods_name(i));

    text(660, 76, 17, C_DIM, "nexusmods.com/silenthill4/mods");
    if (!nexus_have_key()) {
        wrap_text(660, 110, 18, C_DIM,
                  "Add your own API key to sh4.ini as nexus_key=... (the API Access page on your Nexus "
                  "account). Then reopen this screen.");
    } else if (nx_count == 0 && !nx_list_done) {
        text(660, 110, 20, C_DIM, "Asking Nexus...");
    } else if (nx_count == 0) {
        wrap_text(660, 110, 18, C_DIM, nx_list_err[0] ? nx_list_err : "No mods came back.");
    } else {
        int first = sel >= 12 ? sel - 11 : 0;
        for (int i = first; i < nx_count && i < first + 12; i++) {
            char row[220];
            snprintf(row, sizeof row, "%s%s", i == sel ? "> " : "  ", nx_mods[i].name);
            text(660, 110 + (i - first) * 30, 20, i == sel ? C_SEL : C_TEXT, row);
            char by[120];
            snprintf(by, sizeof by, "%.24s", nx_mods[i].author);
            text(660 + text_w(20, row) + 14, 113 + (i - first) * 30, 15, C_DIM, by);
        }
    }
    if (nx_job_done && nx_job.err[0]) wrap_text(70, 560, 18, C_DIM, nx_job.err);
    else if (nx_note[0]) wrap_text(70, 560, 18, C_DIM, nx_note);
    text_c(668, 22, C_DIM, "A: download and install   N: open the page   R: refresh   B: back");
}

static void mods_screen(void) {
    int sel = 0, dirty = 1, last_list = -1;
    nx_job_done = 0;
    nx_job.err[0] = 0;
    nx_note[0] = 0;
    if (nexus_have_key() && nx_count == 0) {
        nx_list_done = 0;
        nx_list_err[0] = 0;
        SDL_DetachThread(SDL_CreateThread(nx_list_thread, "nexus-list", NULL));
    }
    fade(1);
    for (;;) {
        if (dirty) { draw_mods(sel); dirty = 0; }
        present();
        int in = poll_input();
        if (in == IN_DROP) {
            install_mod(drop_file);
            mods_init();
            dirty = 1;
            continue;
        }
        if (nx_list_done != last_list) { last_list = nx_list_done; dirty = 1; }
        if (nx_job_done) {
            char name[128] = "", err[600] = "";
            if (nx_job.ok) {
                if (mods_install(nx_job.zip, name, sizeof name, err, sizeof err)) {
                    rt_log("mods: installed %s from %s", name, nx_job.zip);
                    mods_init();
                    snprintf(nx_note, sizeof nx_note, "Installed \"%s\". It is in mods/ now.", name);
                } else
                    snprintf(nx_note, sizeof nx_note, "%s", err);
                remove(nx_job.zip);
            } else
                snprintf(nx_note, sizeof nx_note, "%s", nx_job.err);
            nx_job_done = 0;
            dirty = 1;
        }
        if (in == IN_NONE) continue;
        if (in == IN_BACK || in == IN_MODS) break;
        if (in == IN_REFRESH) {
            nx_count = 0;
            nx_list_done = 0;
            nx_list_err[0] = 0;
            if (nexus_have_key()) SDL_DetachThread(SDL_CreateThread(nx_list_thread, "nexus-list", NULL));
            dirty = 1;
            continue;
        }
        if (in == IN_PAGE) {
            if (nx_count) {
                char url[300];
                nexus_page_url(nx_mods[sel].id, url, sizeof url);
                SDL_OpenURL(url);
                snprintf(nx_note, sizeof nx_note, "Opened the page in your browser: download the file there and "
                                                  "drop the .zip on this window.");
            } else
                snprintf(nx_note, sizeof nx_note, "Open https://www.nexusmods.com/silenthill4/mods in your browser.");
            dirty = 1;
            continue;
        }
        if (in == IN_DOWN) {
            if (nx_count) sel = (sel + 1) % nx_count;
            dirty = 1;
            continue;
        }
        if (in == IN_UP) {
            if (nx_count) sel = (sel + nx_count - 1) % nx_count;
            dirty = 1;
            continue;
        }
        if (in == IN_OK && nx_count && sel < nx_count) {
            const char *tmp = getenv("TEMP");
            if (!tmp || !*tmp) tmp = getenv("TMP");
            if (!tmp || !*tmp) tmp = ".";
            char safe[128];
            size_t o = 0;
            for (const char *q = nx_mods[sel].name; *q && o < sizeof safe - 1; q++) {
                char c = *q;
                if (strchr("<>:\"/\\|?*", c)) c = '_';
                safe[o++] = c;
            }
            safe[o] = 0;
            if (!safe[0]) snprintf(safe, sizeof safe, "mod-%d", nx_mods[sel].id);
            snprintf(nx_job.zip, sizeof nx_job.zip, "%s/%s.zip", tmp, safe);
            nx_job.mod = nx_mods[sel].id;
            nx_job.ok = 0;
            nx_job.err[0] = 0;
            snprintf(nx_note, sizeof nx_note, "Downloading \"%s\"...", nx_mods[sel].name);
            nx_job_done = 0;
            SDL_DetachThread(SDL_CreateThread(nx_dl_thread, "nexus-download", NULL));
            dirty = 1;
            continue;
        }
        if (in == IN_LEFT || in == IN_RIGHT) dirty = 1;
    }
    fade(0);
}
#else
static void mods_screen(void) {}
#endif

/* ---- the menu ---- */

static const char *menu_items[] = {"Play", "Options", "Mods", "Credits"};
#define N_ITEMS (int)(sizeof menu_items / sizeof *menu_items)

static void draw_menu(int sel) {
    for (int i = 0; i < CW * CH; i++) canvas[i] = C_BG;
    if (logo_px) image(logo_px, logo_w, logo_h, (CW - 820) / 2, 150, 820, 820 * logo_h / logo_w);
    text_c(300, 24, C_DIM, PORT_NAME);
    for (int i = 0; i < N_ITEMS; i++) text_c(440 + i * 62, 42, i == sel ? C_SEL : C_TEXT, menu_items[i]);
    text_c(680, 22, C_DIM, "Arrows: move   A: select   M: mods");
}

static void splash(const uint8_t *img, const uint8_t *end) {
    int w, h, pressed = 0;
    uint32_t *px = decode(img, end, &w, &h);
    if (!px) return;
    for (int i = 0; i < CW * CH; i++) canvas[i] = 0;
    int dw = CW, dh = CW * h / w;
    if (dh > CH) dh = CH, dw = CH * w / h;
    image(px, w, h, (CW - dw) / 2, (CH - dh) / 2, dw, dh);
    free(px);
    fade(1);
    Uint64 t0 = SDL_GetTicks64();
    while (SDL_GetTicks64() - t0 < 4000) {
        poll_input();
        present();
    }
    static uint32_t pic[CW * CH];
    memcpy(pic, canvas, sizeof pic);
    /* A press made while "Press A" is still fading in counts; it used to be dropped and the screen then waited
     * for a second one. */
    for (Uint64 t1 = SDL_GetTicks64(), t = 0;;) {
        int in = poll_input();
        t = SDL_GetTicks64() - t1;
        if (in == IN_OK || in == IN_BACK) pressed = 1;
        int k = t >= FADE_MS ? 256 : (int)(t * 256 / FADE_MS);
        memcpy(canvas, pic, sizeof pic);
        text_c(CH - 34, 32, rgb(235 * k >> 8, 235 * k >> 8, 235 * k >> 8), "Press A");
        present();
        if (pressed && t >= FADE_MS) break;
    }
    fade(0);
}

void launcher_run(void) {
    char saved[512];
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    game_ini(saved, sizeof saved, 0);
    if (!g_game_dir[0] && saved[0] && chdir(saved) == 0) snprintf(g_game_dir, sizeof g_game_dir, "%s", saved);
    sh4_settings_load();   /* sh4.ini lives in the game folder once there is one */
    options_load();
#ifdef _WIN32
    nexus_set_key(g_settings.nexus_key);
#endif

    win = SDL_CreateWindow("Silent Hill 4: The Room", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
    if (!win) {
        rt_log("launcher: %s", SDL_GetError());
        return;
    }
    SDL_SetWindowMinimumSize(win, 640, 360);
    rt_log("launcher: open, %s%s%s\n", g_game_dir[0] ? g_game_dir : "no game folder yet",
           getenv("SH4_KEYS") ? "; keys " : "", getenv("SH4_KEYS") ? getenv("SH4_KEYS") : "");
    int fw, fh;
    font_px = decode(font_bmp, font_bmp_end, &font_w, &fh);
    for (int i = 0; font_px && i < font_w * fh; i++) font_px[i] = font_px[i] << 24 | 0xFFFFFFu;  /* the atlas is coverage */
    logo_px = decode(logo_bmp, logo_bmp_end, &logo_w, &logo_h);
    splash(splash_bmp, splash_bmp_end);
    rt_log("launcher: splash done\n");
    draw_menu(0);
    if (getenv("SH4_DROP")) {
        launcher_drop(getenv("SH4_DROP"));
        draw_menu(0);
    }
    fade(1);
    rt_log("launcher: menu up\n");
    for (int sel = 0, dirty = 0;;) {
        if (dirty) { draw_menu(sel); dirty = 0; }
        present();
        int in = poll_input();
        if (in == IN_DROP) {
            launcher_drop(drop_file);
            draw_menu(sel);
            continue;
        }
        if (in == IN_UP || in == IN_DOWN) sel = (sel + (in == IN_UP ? N_ITEMS - 1 : 1)) % N_ITEMS, dirty = 1;
        if (in == IN_MODS) {
            fade(0);
            mods_screen();
            draw_menu(sel);
            fade(1);
            continue;
        }
        if (in == IN_BACK) {
            rt_log("launcher: closed from the menu\n");
            exit(0);
        }
        if (in == IN_OK && sel == 0) {
            if (!g_game_dir[0]) {
                row_note = "No game folder yet: Options > Game folder";
                fade(0);
                options();
                draw_menu(sel);
                fade(1);
                continue;
            }
#ifdef PORT_LAUNCHER_ONLY
            /* This build has no recompiled game code. The copy's own NoCD executable is how an imported install
             * runs without one; the launcher's work is done once it is running. */
            if (sh4_start_original(g_game_dir)) {
                fade(0);
                exit(0);
            }
            say("Play", "This launcher has no recompiled game code.\n\nBuild the port from your own copy "
                        "(docs/BUILD-WINDOWS.md), or use the copy's own SILENT HILL 4.exe in the game folder.");
            continue;
#else
            rt_log("launcher: play, starting the port from %s\n", g_game_dir);
            sh4_settings_save();
            fade(0);
            SDL_DestroyWindow(win);
            win = NULL;
            free(font_px), font_px = NULL;
            free(logo_px), logo_px = NULL;
            return;
#endif
        }
        if (in == IN_OK && sel == 1) {
            fade(0);
            options();
            draw_menu(sel);
            fade(1);
        }
        if (in == IN_OK && sel == 2) {
            fade(0);
            mods_screen();
            draw_menu(sel);
            fade(1);
        }
        if (in == IN_OK && sel == 3) {
            fade(0);
            credits();
            draw_menu(sel);
            fade(1);
        }
    }
}
