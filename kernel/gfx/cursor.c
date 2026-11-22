#include "gfx/cursor.h"
#include "gfx/draw.h"

static const char *shape[16] = {
    "X           ", "XX          ", "XRX         ", "XRRX        ", "XRRRX       ", "XRRRRX      ",
    "XRRRRRX     ", "XRRRRRRX    ", "XRRRRRRRX   ", "XRRRRRXXXX  ", "XRRXRRX     ", "XRX XRRX    ",
    "XX  XRRX    ", "X    XRRX   ", "     XRRX   ", "      XX    " };

void cursor_draw(int x, int y) {
    draw_noclip();
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 12; c++)
            if (shape[r][c] != ' ')
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) draw_px(x + c + dx, y + r + dy, C_WHT);
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 12; c++)
            if (shape[r][c] != ' ') draw_px(x + c, y + r, C_BLK);
}
