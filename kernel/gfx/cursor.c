#include "gfx/cursor.h"
#include "gfx/draw.h"
#include "drivers/video.h"

static const char *shape[16] = {
    "X           ", "XX          ", "XRX         ", "XRRX        ", "XRRRX       ", "XRRRRX      ",
    "XRRRRRX     ", "XRRRRRRX    ", "XRRRRRRRX   ", "XRRRRRXXXX  ", "XRRXRRX     ", "XRX XRRX    ",
    "XX  XRRX    ", "X    XRRX   ", "     XRRX   ", "      XX    " };

/* Area touched by cursor_draw: 12x16 shape plus a 1 px white outline = 14x18, origin at (x-1, y-1). */
#define CUR_W 14
#define CUR_H 18
static u8 under[CUR_W * CUR_H];
static int under_x, under_y, under_valid;

void cursor_hide(void) {
    if (!under_valid) return;
    u8 *b = video_back();
    for (int r = 0; r < CUR_H; r++) {
        int py = under_y + r;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int c = 0; c < CUR_W; c++) {
            int px = under_x + c;
            if (px >= 0 && px < SCREEN_W) b[py * SCREEN_W + px] = under[r * CUR_W + c];
        }
    }
    under_valid = 0;
}

void cursor_draw(int x, int y) {
    cursor_hide();
    u8 *b = video_back();
    under_x = x - 1; under_y = y - 1;
    for (int r = 0; r < CUR_H; r++) {
        int py = under_y + r;
        for (int c = 0; c < CUR_W; c++) {
            int px = under_x + c;
            under[r * CUR_W + c] = (py >= 0 && py < SCREEN_H && px >= 0 && px < SCREEN_W) ? b[py * SCREEN_W + px] : 0;
        }
    }
    under_valid = 1;
    draw_noclip();
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 12; c++)
            if (shape[r][c] != ' ')
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) draw_px(x + c + dx, y + r + dy, C_WHT);
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 12; c++)
            if (shape[r][c] != ' ') draw_px(x + c, y + r, C_BLK);
}
