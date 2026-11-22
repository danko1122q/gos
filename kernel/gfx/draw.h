#ifndef GFX_DRAW_H
#define GFX_DRAW_H
#include "lib/types.h"

enum { C_BLK, C_WHT, C_DESK, C_WIN, C_RED, C_GRY, C_DGR, C_BLU, C_GRN, C_YEL, C_CYN, C_LGR, C_DESK2  };

void draw_set_target(u8 *buf);
void draw_noclip(void);
void draw_clip(int x0, int y0, int x1, int y1);
void draw_px(int x, int y, u8 c);
int  draw_getpx(int x, int y);
void draw_rect(int x, int y, int w, int h, u8 c);
void draw_frame(int x, int y, int w, int h, u8 c);
void draw_box(int x, int y, int w, int h, u8 fill, u8 border);
void draw_hline(int x, int y, int w, u8 c);
void draw_vline(int x, int y, int h, u8 c);

#endif
