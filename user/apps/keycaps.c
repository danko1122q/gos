#include "uikit.h"

static const char *rows[4] = { "`1234567890-=", "qwertyuiop[]\\", "asdfghjkl;'", "zxcvbnm,./" };
static const char *rows_s[4] = { "~!@#$%^&*()_+", "QWERTYUIOP{}|", "ASDFGHJKL:\"", "ZXCVBNM<>?" };
static const int roff[4] = { 0, 12, 20, 34 };
#define KW 28
#define KH 26

static void draw(Win *w, const char *text, int lit, int shift) {
    win_begin(w);
    draw_rect(0, 0, w->cw, w->ch, C_WIN);
    draw_box(8, 6, w->cw - 16, 24, C_WHT, C_BLK);
    int skip = 0;
    while (text[skip] && ftext_width(FACE_GENEVA, text + skip) > w->cw - 28) skip++;
    ftext_draw(FACE_GENEVA, 12, 10, text + skip, C_BLK, -1);
    for (int r = 0; r < 4; r++) {
        const char *s = shift ? rows_s[r] : rows[r];
        for (int i = 0; s[i]; i++) {
            int x = 8 + roff[r] + i * (KW + 2), y = 40 + r * (KH + 2);
            int on = lit && (s[i] == lit || rows[r][i] == lit || rows_s[r][i] == lit);
            draw_box(x, y, KW, KH, on ? C_BLK : C_WHT, C_BLK);
            char t[2] = { s[i], 0 };
            ftext_draw_center(FACE_GENEVA, x + KW / 2, y + 5, t, on ? C_WHT : C_BLK, -1);
        }
    }
    int sp = lit == ' ';
    draw_box(8 + 80, 40 + 4 * (KH + 2), 200, KH, sp ? C_BLK : C_WHT, C_BLK);
    win_present(w);
}

int main(void) {
    static Win w; char text[128] = ""; int lit = 0, shift = 0, dirty = 1, quit = 0; u32 lit_at = 0; GEvent e;
    if (win_create(&w, "Key Caps", -1, 0, 8 * 2 + 13 * (KW + 2) + 24, 40 + 5 * (KH + 2) + 22 + 20 + 8) < 0) return 1;
    win_set_info(&w, "Type to see keys");
    while (!quit) {
        if (dirty) { draw(&w, text, lit, shift); dirty = 0; }
        int r = win_next(&w, &e, 150);
        if (r < 0) break;
        if (r == 0) { if (lit && gos_ticks() - lit_at > 20) { lit = 0; dirty = 1; } continue; }
        do {
            switch (e.type) {
            case GE_KEY: {
                int k = e.a; shift = e.b & MOD_SHIFT ? 1 : 0;
                int n = (int)strlen(text);
                if (k == '\b') { if (n) text[n - 1] = 0; lit = 0; }
                else if (k >= 32 && k < 127) { lit = k; lit_at = gos_ticks(); if (n < 126) { text[n] = (char)k; text[n + 1] = 0; } }
                dirty = 1; break; }
            case GE_RESIZE: dirty = 1; break;
            case GE_CLOSE: quit = 1; break;
            default: break;
            }
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    win_close(&w);
    return 0;
}
