#include "gui/widget.h"
#include "gui/metrics.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "lib/string.h"

void widget_titlebar(int x, int y, int w, const char *title, int active, int zoomed) {
    int k;
    draw_rect(x + 1, y + 1, w - 2, GUI_TH - 1, C_WHT); draw_rect(x, y + GUI_TH - 1, w, 1, C_BLK);
    if (active) for (k = 5; k <= 15; k += 2) draw_rect(x + 3, y + k, w - 6, 1, C_BLK);
    char t[48];
    int maxw = w - 64;
    if (maxw < 40) maxw = 40;
    ftext_fit(FACE_CHICAGO, title, maxw, t, sizeof t);
    int tw = ftext_width(FACE_CHICAGO, t) + 16;
    draw_rect(x + (w - tw) / 2, y + 2, tw, GUI_TH - 4, C_WHT);
    ftext_draw(FACE_CHICAGO, x + (w - tw) / 2 + 8, y + 3, t, C_BLK, -1);
    if (!active) return;
    draw_box(x + 5, y + 4, 13, 13, C_WHT, C_BLK);
    int zx = x + w - 19, zy = y + 4;
    draw_box(zx, zy, 13, 13, C_WHT, C_BLK);
    if (zoomed) draw_frame(zx + 2, zy + 2, 9, 9, C_BLK); else draw_frame(zx + 2, zy + 2, 6, 6, C_BLK);
}

void widget_infobar(int x, int y, int w, const char *info) {
    draw_rect(x + 1, y, w - 2, GUI_IH, C_WHT); draw_rect(x + 1, y + GUI_IH - 1, w - 2, 1, C_BLK);
    char t[96];
    int maxw = w - 12;
    if (maxw < 24) return;
    ftext_fit(FACE_GENEVA, info, maxw, t, sizeof t);
    ftext_draw_center(FACE_GENEVA, x + w / 2, y + 2, t, C_BLK, -1);
}

static void thumb_geom(int h, int pos, int total, int view, int *ty, int *tl) {
    int track = h - 32;
    if (track < 4) { *ty = 16; *tl = 0; return; }
    if (total <= view || view <= 0) { *ty = 16; *tl = 0; return; }
    int len = track * view / total;
    if (len < 12) len = 12;
    if (len > track) len = track;
    int maxp = total - view;
    if (pos < 0) pos = 0;
    if (pos > maxp) pos = maxp;
    *ty = 16 + (track - len) * pos / maxp;
    *tl = len;
}

void widget_vscroll(int sx, int sy, int sh, int pos, int total, int view) {
    int i, k;
    draw_rect(sx, sy, GUI_SB, sh, C_WHT); draw_rect(sx, sy, 1, sh, C_BLK);
    for (int j = 16; j < sh - 16; j++)
        for (i = 1; i < GUI_SB; i++) if (((i + j) & 1) == 0) draw_px(sx + i, sy + j, C_BLK);
    int ty, tl; thumb_geom(sh, pos, total, view, &ty, &tl);
    if (tl) { draw_box(sx + 2, sy + ty, GUI_SB - 3, tl, C_WHT, C_BLK); }
    draw_rect(sx, sy + 15, GUI_SB, 1, C_BLK); draw_rect(sx, sy + sh - 16, GUI_SB, 1, C_BLK);
    for (k = 0; k < 6; k++) {
        draw_rect(sx + 8 - k, sy + 4 + k, 2 * k + 1, 1, C_BLK);
        draw_rect(sx + 8 - k, sy + sh - 5 - k, 2 * k + 1, 1, C_BLK);
    }
}

int widget_vscroll_part(int h, int pos, int total, int view, int ly, int *grab) {
    if (ly < 16) return SB_UP;
    if (ly >= h - 16) return SB_DOWN;
    int ty, tl; thumb_geom(h, pos, total, view, &ty, &tl);
    if (!tl) return SB_NONE;
    if (ly < ty) return SB_PGUP;
    if (ly >= ty + tl) return SB_PGDN;
    *grab = ly - ty;
    return SB_THUMB;
}

int widget_vscroll_pos(int h, int total, int view, int ly, int grab) {
    int track = h - 32, len = track * view / total;
    if (len < 12) len = 12;
    int span = track - len, maxp = total - view;
    if (span <= 0 || maxp <= 0) return 0;
    int t = ly - grab - 16;
    if (t < 0) t = 0;
    if (t > span) t = span;
    return (t * maxp + span / 2) / span;
}

void widget_sizebox(int x, int y) {
    draw_rect(x, y, GUI_SB, 14, C_WHT); draw_rect(x, y, GUI_SB, 1, C_BLK); draw_rect(x, y, 1, 14, C_BLK);
    for (int j = 0; j < 3; j++)
        for (int t = 0; t < 9 - 4 * j; t++) draw_px(x + 3 + j * 4 + t, y + 12 - t, C_BLK);
}

void widget_button(int x, int y, int w, int h, const char *label, int pressed) {
    draw_rect(x + 2, y + 2, w, h, C_BLK);
    draw_box(x, y, w, h, pressed ? C_BLK : C_WHT, C_BLK);
    ftext_draw_center(FACE_CHICAGO, x + w / 2, y + (h - 16) / 2, label, pressed ? C_WHT : C_BLK, -1);
}

void widget_button_default(int x, int y, int w, int h, const char *label, int pressed) {
    draw_frame(x - 4, y - 4, w + 10, h + 10, C_BLK);
    draw_frame(x - 3, y - 3, w + 8, h + 8, C_BLK);
    draw_px(x - 4, y - 4, C_WHT); draw_px(x + w + 5, y - 4, C_WHT);
    draw_px(x - 4, y + h + 5, C_WHT); draw_px(x + w + 5, y + h + 5, C_WHT);
    widget_button(x, y, w, h, label, pressed);
}
