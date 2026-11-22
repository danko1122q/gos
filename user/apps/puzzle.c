#include "uikit.h"
#include "puzzle_logic.h"

#define T 56
#define OX 8
#define OY 8

static void draw(Win *w, const int *b, int moves, int won) {
    win_begin(w);
    draw_rect(0, 0, w->cw, w->ch, C_WIN);
    for (int i = 0; i < 16; i++) {
        int x = OX + (i % 4) * T, y = OY + (i / 4) * T;
        if (!b[i]) { draw_rect(x, y, T - 2, T - 2, C_DGR); continue; }
        draw_box(x, y, T - 2, T - 2, won ? C_YEL : C_WHT, C_BLK);
        draw_rect(x + T - 4, y + 1, 1, T - 4, C_GRY); draw_rect(x + 1, y + T - 4, T - 4, 1, C_GRY);
        char s[4]; ksnprintf(s, sizeof s, "%d", b[i]);
        ftext_draw_center(FACE_CHICAGO, x + (T - 2) / 2, y + (T - 2 - 16) / 2, s, C_BLK, -1);
    }
    char m[48]; ksnprintf(m, sizeof m, won ? "Solved in %d moves!" : "Moves: %d", moves);
    win_set_info(w, m);
    win_present(w);
}

int main(void) {
    static Win w; int b[16]; unsigned seed = gos_ticks() + 1; int moves = 0, won = 0, dirty = 1, quit = 0;
    if (win_create(&w, "Puzzle", -1, 0, OX * 2 + T * 4 + 18, OY * 2 + T * 4 + 22 + 20 + 1) < 0) return 1;
    win_set_minsize(&w, 200, 200);
    win_set_menu(&w, "Shuffle", "");
    pz_shuffle(b, &seed, 300);
    GEvent e;
    while (!quit) {
        if (dirty) { draw(&w, b, moves, won); dirty = 0; }
        if (win_next(&w, &e, -1) <= 0) break;
        do {
            switch (e.type) {
            case GE_MOUSE_DOWN: {
                int c = (e.a - OX) / T, r = (e.b - OY) / T;
                if (e.a >= OX && e.b >= OY && c < 4 && r < 4 && !won && pz_move(b, r * 4 + c)) {
                    moves++; if (pz_is_solved(b)) { won = 1; gos_beep(880, 80); }
                } else if (!won) gos_beep(0, 0);
                dirty = 1; break; }
            case GE_CMD: pz_shuffle(b, &seed, 300); moves = 0; won = 0; dirty = 1; break;
            case GE_RESIZE: dirty = 1; break;
            case GE_CLOSE: quit = 1; break;
            default: break;
            }
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    win_close(&w);
    return 0;
}
