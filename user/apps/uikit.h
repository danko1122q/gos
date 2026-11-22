#ifndef USER_UIKIT_H
#define USER_UIKIT_H
#include "gos.h"

static inline int ui_in(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

static inline void ui_radio(int x, int y, const char *label, int on) {
    for (int dy = -6; dy <= 6; dy++)
        for (int dx = -6; dx <= 6; dx++) {
            int d = dx * dx + dy * dy;
            if (d <= 36) draw_px(x + 7 + dx, y + 8 + dy, d >= 25 ? C_BLK : C_WHT);
            if (on && d <= 9) draw_px(x + 7 + dx, y + 8 + dy, C_BLK);
        }
    ftext_draw(FACE_GENEVA, x + 20, y, label, C_BLK, -1);
}
static inline void ui_check(int x, int y, const char *label, int on) {
    draw_box(x, y + 1, 14, 14, C_WHT, C_BLK);
    if (on) { for (int k = 0; k < 10; k++) { draw_px(x + 2 + k, y + 3 + k, C_BLK); draw_px(x + 11 - k, y + 3 + k, C_BLK); } }
    ftext_draw(FACE_GENEVA, x + 20, y, label, C_BLK, -1);
}
static inline void ui_arrow(int x, int y, int up) {
    draw_box(x, y, 16, 14, C_WHT, C_BLK);
    for (int k = 0; k < 5; k++) draw_rect(x + 8 - k, y + (up ? 3 + k : 10 - k), 2 * k + 1, 1, C_BLK);
}
static inline u32 ui_rand(u32 *st) { *st = *st * 1103515245u + 12345u; return (*st >> 16) & 0x7FFF; }
#endif
