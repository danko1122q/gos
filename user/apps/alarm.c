#include "uikit.h"

static const int sn[60] = { 0,10,21,31,41,50,59,67,74,81,87,92,95,98,100,100,100,98,95,92,87,81,74,67,59,50,41,31,21,10,
                            0,-10,-21,-31,-41,-50,-59,-67,-74,-81,-87,-92,-95,-98,-100,-100,-100,-98,-95,-92,-87,-81,-74,-67,-59,-50,-41,-31,-21,-10 };
static const char *dn[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *mn[] = { "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static void hand(int cx, int cy, int len, int m60, u8 col) {
    for (int t = 0; t <= len; t++) draw_px(cx + sn[m60] * t / 100, cy - sn[(m60 + 15) % 60] * t / 100, col);
}

int main(void) {
    static Win w; GEvent e; int al_on = 0, al_h = 7, al_m = 0, quit = 0, rung = 0, ring = 0, dirty = 1;
    if (win_create(&w, "Alarm Clock", -1, 0, 240, 290) < 0) return 1;
    win_set_minsize(&w, 240, 290);
    while (!quit) {
        if (dirty) {
            DateTime t; epoch_to_dt(gos_time(), &t);
            win_begin(&w);
            draw_rect(0, 0, w.cw, w.ch, ring && (t.sec & 1) ? C_YEL : C_WIN);
            int cx = w.cw / 2, cy = 80;
            for (int r = 0; r <= 62; r++) for (int a = 0; a < 60; a++) if (r == 62 || (r == 60 && a % 5 == 0)) draw_px(cx + sn[a] * r / 100, cy - sn[(a + 15) % 60] * r / 100, C_BLK);
            draw_rect(cx - 2, cy - 2, 5, 5, C_BLK);
            hand(cx, cy, 36, (t.hour % 12) * 5 + t.min / 12, C_BLK); hand(cx, cy, 52, t.min, C_BLK); hand(cx, cy, 56, t.sec, C_RED);
            char b[48];
            ksnprintf(b, sizeof b, "%02d:%02d:%02d", t.hour, t.min, t.sec); ftext_draw_center(FACE_GENEVA, cx, 156, b, C_BLK, -1);
            ksnprintf(b, sizeof b, "%s, %d %s %d", dn[t.wday % 7], t.day, mn[t.mon % 13], t.year); ftext_draw_center(FACE_GENEVA, cx, 176, b, C_BLK, -1);
            draw_rect(8, 200, w.cw - 16, 1, C_BLK);
            ui_check(12, 212, "Alarm on", al_on);
            ksnprintf(b, sizeof b, "%02d:%02d", al_h, al_m); ftext_draw(FACE_GENEVA, 130, 212, b, C_BLK, -1);
            ui_arrow(130, 232, 1); ui_arrow(130, 250, 0); ui_arrow(170, 232, 1); ui_arrow(170, 250, 0);
            ftext_draw(FACE_GENEVA, 12, 232, "Hour   (left)", C_DGR, -1); ftext_draw(FACE_GENEVA, 12, 250, "Minute (right)", C_DGR, -1);
            win_set_info(&w, ring ? "ALARM! Click the window to stop it" : al_on ? "Alarm set" : "Alarm off");
            win_present(&w); dirty = 0;
            if (al_on && !rung && t.hour == al_h && t.min == al_m) { rung = 1; ring = 1; }
            if (!(al_on && t.hour == al_h && t.min == al_m)) rung = 0;
            if (ring) gos_beep(988, 150);
        }
        int r = win_next(&w, &e, 1000);
        if (r < 0) break;
        if (r == 0) { dirty = 1; continue; }
        do {
            switch (e.type) {
            case GE_MOUSE_DOWN:
                if (ring) { ring = 0; }
                else if (ui_in(e.a, e.b, 12, 212, 110, 16)) al_on = !al_on;
                else if (ui_in(e.a, e.b, 130, 232, 16, 14)) al_h = (al_h + 1) % 24;
                else if (ui_in(e.a, e.b, 130, 250, 16, 14)) al_h = (al_h + 23) % 24;
                else if (ui_in(e.a, e.b, 170, 232, 16, 14)) al_m = (al_m + 1) % 60;
                else if (ui_in(e.a, e.b, 170, 250, 16, 14)) al_m = (al_m + 59) % 60;
                dirty = 1; break;
            case GE_KEY: ring = 0; dirty = 1; break;
            case GE_RESIZE: dirty = 1; break;
            case GE_CLOSE: quit = 1; break;
            default: break;
            }
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    win_close(&w);
    return 0;
}
