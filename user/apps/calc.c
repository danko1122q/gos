#include "uikit.h"
#include "calc_engine.h"

static const struct { const char *l; int k; } keys[5][4] = {
    { {"C", 'C'}, {"\xF1", 'n'}, {"%", '%'}, {"\xF6", '/'} },
    { {"7", '7'}, {"8", '8'}, {"9", '9'}, {"x", '*'} },
    { {"4", '4'}, {"5", '5'}, {"6", '6'}, {"-", '-'} },
    { {"1", '1'}, {"2", '2'}, {"3", '3'}, {"+", '+'} },
    { {"0", '0'}, {".", '.'}, {"\xFB", 'r'}, {"=", '='} },
};
#define BX 8
#define BY 46
#define BW 40
#define BH 32
#define GAP 4

static void draw(Win *w, Calc *c, int pr, int pc) {
    win_begin(w);
    draw_rect(0, 0, w->cw, w->ch, C_WIN);
    draw_box(BX, 6, w->cw - 2 * BX, 32, C_WHT, C_BLK);
    char d[32]; calc_display(c, d, sizeof d);
    ftext_draw(FACE_CHICAGO, w->cw - BX - 8 - ftext_width(FACE_CHICAGO, d), 14, d, C_BLK, -1);
    if (c->op) ftext_draw(FACE_CHICAGO, BX + 6, 14, c->op == '*' ? "x" : c->op == '/' ? "\xF6" : (char[2]){ (char)c->op, 0 }, C_DGR, -1);
    for (int r = 0; r < 5; r++)
        for (int k = 0; k < 4; k++)
            widget_button(BX + k * (BW + GAP), BY + r * (BH + GAP), BW, BH, keys[r][k].l, r == pr && k == pc);
    win_present(w);
}

int main(void) {
    static Win w; static Calc c;
    if (win_create(&w, "Calculator", -1, 0, 210, 262) < 0) return 1;
    win_set_minsize(&w, 210, 262);
    calc_init(&c);
    int pr = -1, pc = -1, dirty = 1, quit = 0; GEvent e;
    win_set_info(&w, "6 decimal places");
    while (!quit) {
        if (dirty) { draw(&w, &c, pr, pc); dirty = 0; }
        if (win_next(&w, &e, -1) <= 0) break;
        do {
            switch (e.type) {
            case GE_MOUSE_DOWN: {
                int k = (e.a - BX) / (BW + GAP), r = (e.b - BY) / (BH + GAP);
                if (e.a >= BX && e.b >= BY && k < 4 && r < 5 && (e.a - BX) % (BW + GAP) < BW && (e.b - BY) % (BH + GAP) < BH) {
                    pr = r; pc = k; calc_key(&c, keys[r][k].k);
                }
                dirty = 1; break; }
            case GE_MOUSE_UP: pr = pc = -1; dirty = 1; break;
            case GE_KEY:
                if (e.a == 27 || e.a == 'c') calc_key(&c, 'C');
                else if (e.a == 'r') calc_key(&c, 'r');
                else if (e.a == '\n') calc_key(&c, '=');
                else calc_key(&c, e.a);
                dirty = 1; break;
            case GE_RESIZE: dirty = 1; break;
            case GE_CLOSE: quit = 1; break;
            default: break;
            }
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    win_close(&w);
    return 0;
}
