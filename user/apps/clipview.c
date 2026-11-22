#include "uikit.h"

int main(void) {
    static Win w; char *buf = NULL; u32 len = 0, last = 0xFFFFFFFFu; int sc = 0, dirty = 1, quit = 0; GEvent e;
    if (win_create(&w, "Clipboard", -1, 0, 380, 240) < 0) return 1;
    win_set_minsize(&w, 200, 100);
    while (!quit) {
        if (dirty) {
            win_begin(&w);
            draw_rect(0, 0, w.cw, w.ch, C_WHT);
            int cols = (w.cw - 8) / 8, rows = w.ch / 16, row = 0, col = 0, total = 1;
            if (cols < 1) cols = 1;
            for (u32 i = 0; i < len; i++) {
                char ch = buf[i];
                if (ch == '\n' || col >= cols) { total++; col = 0; if (ch == '\n') continue; }
                col++;
            }
            if (sc > total - rows) sc = total - rows;
            if (sc < 0) sc = 0;
            col = 0;
            for (u32 i = 0; i < len; i++) {
                char ch = buf[i];
                if (ch == '\n' || col >= cols) { row++; col = 0; if (ch == '\n') continue; }
                if (row >= sc && row - sc < rows && ch >= 32 && ch < 127) { char t[2] = { ch, 0 }; text_draw(4 + col * 8, (row - sc) * 16, t, C_BLK, -1); }
                col++;
            }
            win_set_scroll(&w, sc, total, rows > 0 ? rows : 1);
            win_present(&w); dirty = 0;
        }
        int r = win_next(&w, &e, 500);
        if (r < 0) break;
        if (r == 0) {
            int n = gos_clip_get(NULL, 0);
            if (n >= 0 && (u32)n != last) {
                kfree(buf); buf = NULL;
                if (gos_clip_text(&buf, &len) == 0) { last = len; char i[48]; ksnprintf(i, sizeof i, "%u bytes on the clipboard", len); win_set_info(&w, i); dirty = 1; }
            } else if (n >= 0 && n > 0) {
                char *nb; u32 nl;
                if (gos_clip_text(&nb, &nl) == 0) { if (nl != len || memcmp(nb, buf, nl)) { kfree(buf); buf = nb; len = nl; dirty = 1; } else kfree(nb); }
            }
            continue;
        }
        do {
            if (e.type == GE_SCROLL) { sc = e.a; dirty = 1; }
            else if (e.type == GE_RESIZE) dirty = 1;
            else if (e.type == GE_CLOSE) quit = 1;
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    win_close(&w);
    kfree(buf);
    return 0;
}
