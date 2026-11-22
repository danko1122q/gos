#include "gfx/icons.h"
#include "gfx/draw.h"

#define rect draw_rect
#define box  draw_box
#define BLK  C_BLK
#define WHT  C_WHT

static void page(int x, int y) {
    for (int r = 0; r < 31; r++) {
        int rx = r < 8 ? 19 + r : 27;
        rect(x + 4, y + r, rx - 3, 1, WHT);
        draw_px(x + 4, y + r, BLK); draw_px(x + rx, y + r, BLK);
    }
    rect(x + 4, y, 16, 1, BLK); rect(x + 4, y + 30, 24, 1, BLK);
    rect(x + 19, y, 1, 9, BLK); rect(x + 19, y + 8, 9, 1, BLK);
}

void icon_draw(int x, int y, int t) {
    switch (t) {
    case ICON_FILE: case ICON_TEXT:
        page(x, y);
        if (t == ICON_TEXT) for (int i = 0; i < 4; i++) rect(x + 8, y + 14 + i * 4, 16, 1, BLK);
        break;
    case ICON_FOLDER:
        box(x + 2, y + 8, 28, 22, WHT, BLK);
        rect(x + 3, y + 4, 12, 5, WHT); rect(x + 2, y + 4, 1, 5, BLK);
        rect(x + 2, y + 4, 14, 1, BLK); rect(x + 15, y + 4, 1, 5, BLK);
        rect(x + 3, y + 8, 12, 1, WHT);
        rect(x + 4, y + 11, 24, 1, BLK);
        rect(x + 30, y + 9, 1, 22, BLK); rect(x + 3, y + 30, 28, 1, BLK);
        break;
    case ICON_PROGRAM:
        box(x + 1, y + 2, 30, 27, WHT, BLK);
        rect(x + 1, y + 2, 30, 6, BLK);
        rect(x + 9, y + 12, 3, 13, BLK); rect(x + 15, y + 12, 3, 13, BLK); rect(x + 21, y + 12, 3, 13, BLK);
        rect(x + 9, y + 12, 15, 2, BLK);
        break;
    case ICON_DRIVE:
        box(x + 1, y + 7, 30, 19, WHT, BLK);
        rect(x + 32 - 1, y + 8, 1, 19, BLK); rect(x + 2, y + 26, 30, 1, BLK);
        rect(x + 5, y + 13, 16, 2, BLK);
        rect(x + 24, y + 13, 3, 3, BLK);
        rect(x + 5, y + 19, 22, 1, BLK);
        break;
    case ICON_FLOPPY:
        box(x + 2, y + 1, 28, 30, WHT, BLK);
        box(x + 8, y + 2, 16, 9, WHT, BLK); rect(x + 17, y + 3, 3, 7, BLK);
        box(x + 6, y + 15, 20, 15, WHT, BLK);
        rect(x + 9, y + 19, 14, 1, BLK); rect(x + 9, y + 23, 14, 1, BLK);
        break;
    case ICON_TERM:
        box(x + 4, y + 1, 24, 24, WHT, BLK);
        box(x + 7, y + 4, 18, 14, BLK, BLK);
        for (int k = 0; k < 3; k++) { rect(x + 9 + k, y + 7 + k, 2, 1, WHT); rect(x + 9 + k, y + 13 - k, 2, 1, WHT); }
        rect(x + 14, y + 13, 6, 1, WHT);
        rect(x + 15, y + 21, 8, 1, BLK);
        rect(x + 28, y + 2, 1, 25, BLK); rect(x + 5, y + 25, 24, 1, BLK);
        box(x + 2, y + 27, 28, 4, WHT, BLK);
        break;
    case ICON_MONITOR:
        box(x + 1, y + 2, 30, 27, WHT, BLK);
        rect(x + 5, y + 18, 5, 8, BLK); rect(x + 12, y + 12, 5, 14, BLK);
        rect(x + 19, y + 7, 5, 19, BLK); rect(x + 26, y + 14, 3, 12, BLK);
        rect(x + 4, y + 26, 26, 1, BLK);
        break;
    case ICON_APP:
        box(x + 1, y + 2, 30, 27, WHT, BLK);
        for (int k = 0; k < 3; k++) rect(x + 3, y + 4 + k * 2, 26, 1, BLK);
        rect(x + 1, y + 9, 30, 1, BLK);
        box(x + 7, y + 14, 18, 11, WHT, BLK);
        break;
    case ICON_TRASH: case ICON_TRASH_FULL:
        box(x + 4, y + 3, 24, 4, WHT, BLK);
        box(x + 6, y + 7, 20, 24, WHT, BLK);
        for (int i = 0; i < 4; i++) rect(x + 10 + i * 4, y + 11, 1, 16, BLK);
        if (t == ICON_TRASH_FULL) {
            box(x + 8, y, 9, 8, WHT, BLK); box(x + 15, y + 1, 9, 7, WHT, BLK);
            rect(x + 10, y + 3, 5, 1, BLK); rect(x + 17, y + 4, 5, 1, BLK);
        } else box(x + 12, y, 8, 4, WHT, BLK);
        break;
    }
}

void icon_draw_small(int x, int y, int t) {
    if (t == ICON_FOLDER) { box(x, y + 2, 5, 4, WHT, BLK); box(x, y + 5, 12, 9, WHT, BLK); }
    else {
        box(x + 1, y + 1, 9, 13, WHT, BLK);
        if (t == ICON_PROGRAM || t == ICON_APP) rect(x + 1, y + 1, 9, 4, BLK);
        if (t == ICON_TERM) rect(x + 2, y + 2, 7, 11, BLK);
        if (t == ICON_DRIVE) { rect(x + 3, y + 5, 5, 2, BLK); rect(x + 3, y + 9, 5, 2, BLK); }
        if (t == ICON_TRASH || t == ICON_TRASH_FULL) rect(x + 3, y + 4, 1, 8, BLK), rect(x + 6, y + 4, 1, 8, BLK);
        if (t == ICON_MONITOR) { rect(x + 3, y + 8, 2, 5, BLK); rect(x + 6, y + 4, 2, 9, BLK); }
    }
}

static void draw_inverted(int x, int y, int w, int h, int type, void (*fn)(int, int, int)) {
    u8 under[32 * 32];
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) { int p = draw_getpx(x + i, y + j); under[j * w + i] = p < 0 ? 0 : (u8)p; }
    draw_rect(x, y, w, h, C_RED);
    fn(x, y, type);
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int p = draw_getpx(x + i, y + j);
        if (p < 0) continue;
        if (p == C_RED) draw_px(x + i, y + j, under[j * w + i]);
        else if (p == C_BLK) draw_px(x + i, y + j, C_WHT);
        else if (p == C_WHT) draw_px(x + i, y + j, C_BLK);
    }
}
void icon_draw_sel(int x, int y, int type, int sel) { if (sel) draw_inverted(x, y, 32, 32, type, icon_draw); else icon_draw(x, y, type); }
void icon_draw_small_sel(int x, int y, int type, int sel) { if (sel) draw_inverted(x, y, 14, 14, type, icon_draw_small); else icon_draw_small(x, y, type); }
