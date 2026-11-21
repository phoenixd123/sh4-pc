// Couche plateforme (PC : src/pc/, Vita : src/vita/).
#pragma once
#include <SDL.h>

void plat_show_window(void);
// Returns the Win32 button ID, or zero if the dialogue could not be shown.
int plat_message_box(const char *title, const char *text, unsigned type);
int plat_pump_events(void);   // 1 if the user asked to quit
int plat_width(void);
int plat_height(void);
/* sh4.ini's look settings, read by the renderer (d3d8/draw.c). */
int plat_filter_mode(void);   /* 0 off, 1 CRT, 2 VHS */
int plat_aniso(void);         /* anisotropic filtering, 1 = off */
SDL_Window *plat_sdl_window(void);
