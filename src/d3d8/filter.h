/* Post-process filters over the presented frame (sh4.ini filter=): Off, CRT, VHS. The pass draws the game's
 * display texture where the plain blit would have put it, so the letterbox stays the blit's. */
#pragma once
#include "gl.h"

int filter_init(void);  /* 1 when the shader is ready; a failure leaves the plain blit in place */
void filter_draw(GLuint src, int mode, float seconds);
