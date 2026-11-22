#include "gfx/draw.h"
#include "drivers/video.h"

static u8 *dst;
static int cx0, cy0, cx1, cy1;

void draw_set_target(u8 *buf) { dst = buf; }
void draw_noclip(void) { cx0 = 0; cy0 = 0; cx1 = SCREEN_W; cy1 = SCREEN_H; }
void draw_clip(int x0, int y0, int x1, int y1) { cx0 = x0; cy0 = y0; cx1 = x1; cy1 = y1; }

void draw_px(int x, int y, u8 c) {
    if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) dst[y * SCREEN_W + x] = c;
}
int draw_getpx(int x, int y) {
    return (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) ? dst[y * SCREEN_W + x] : -1;
}
void draw_rect(int x, int y, int w, int h, u8 c) {
    int x1 = x + w, y1 = y + h;
    if (x < cx0) x = cx0;
    if (y < cy0) y = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    for (int j = y; j < y1; j++)
        for (int i = x; i < x1; i++) dst[j * SCREEN_W + i] = c;
}
void draw_frame(int x, int y, int w, int h, u8 c) {
    draw_rect(x, y, w, 1, c); draw_rect(x, y + h - 1, w, 1, c);
    draw_rect(x, y, 1, h, c); draw_rect(x + w - 1, y, 1, h, c);
}
void draw_box(int x, int y, int w, int h, u8 fill, u8 border) {
    draw_rect(x, y, w, h, fill); draw_frame(x, y, w, h, border);
}
void draw_hline(int x, int y, int w, u8 c) { draw_rect(x, y, w, 1, c); }
void draw_vline(int x, int y, int h, u8 c) { draw_rect(x, y, 1, h, c); }
