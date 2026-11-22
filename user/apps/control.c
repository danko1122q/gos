#include "uikit.h"
#include "gfx/patterns.h"

enum { P_GEN, P_MOUSE, P_KBD, P_SND, NP };
static const char *pname[NP] = { "General", "Mouse", "Keyboard", "Sound" };
static const u8 pic[NP] = { ICON_DRIVE, ICON_APP, ICON_TEXT, ICON_MONITOR };
static const char *ccol[GOS_DESK_COLORS] = { "classic", "green", "blue", "grey", "purple", "teal" };
static const u8 cshade[GOS_DESK_COLORS][3] = { {255,255,255}, {0,120,72}, {0,72,140}, {96,96,96}, {100,60,140}, {0,110,120} };

static int pane, tf[5], tf_init, dbl_hit, kbd_n;
static char kbd_text[40];
static const int dim[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static void load_time(void) { DateTime t; epoch_to_dt(gos_time(), &t); tf[0] = t.day; tf[1] = t.mon; tf[2] = t.year; tf[3] = t.hour; tf[4] = t.min; tf_init = 1; }
static void apply_time(void) {
    int d = tf[0], m = tf[1], y = tf[2]; int mx = dim[m] + (m == 2 && (y % 4 == 0) ? 1 : 0);
    if (d > mx) d = mx;
    if (gos_settime(dt_to_epoch(y, m, d, tf[3], tf[4], 0)) < 0) win_msgbox("Control Panel", "Year must be 2000-2099.", "");
}
static void radios(int x, int y, const char *const *labels, int n, int sel) { for (int i = 0; i < n; i++) ui_radio(x, y + i * 22, labels[i], i == sel); }

static void draw(Win *w) {
    win_begin(w); draw_rect(0, 0, w->cw, w->ch, C_WIN);
    draw_rect(110, 0, 1, w->ch, C_BLK);
    for (int i = 0; i < NP; i++) {
        int y = 6 + i * 52;
        if (i == pane) draw_rect(2, y - 2, 106, 50, C_LGR);
        icon_draw(40, y, pic[i]); ftext_draw_center(FACE_GENEVA, 56, y + 34, pname[i], C_BLK, -1);
    }
    int x = 124; char b[48];
    if (pane == P_GEN) {
        int sp = gos_setting_get(SET_DESK_PATTERN), sc = gos_setting_get(SET_DESK_COLOR);
        ftext_draw(FACE_GENEVA | FS_BOLD, x, 6, "Desktop Pattern", C_BLK, -1);
        for (int i = 0; i < GOS_DESK_PATTERNS; i++) {
            int px = x + (i % 4) * 62, py = 26 + (i / 4) * 44;
            draw_box(px, py, 56, 38, C_DESK, i == sp ? C_RED : C_BLK);
            for (int yy = 0; yy < 36; yy++) for (int xx = 0; xx < 54; xx++) if ((desk_patterns[i][yy & 7] >> (7 - (xx & 7))) & 1) draw_px(px + 1 + xx, py + 1 + yy, C_DESK2);
            if (i == sp) draw_frame(px - 1, py - 1, 58, 40, C_RED);
        }
        ftext_draw(FACE_GENEVA | FS_BOLD, x, 120, "Desktop Color", C_BLK, -1);
        for (int i = 0; i < GOS_DESK_COLORS; i++) {
            draw_box(x + i * 42, 140, 36, 22, C_DESK, i == sc ? C_RED : C_BLK);
            if (i != sc) { draw_rect(x + i * 42 + 1, 141, 34, 20, C_LGR); draw_rect(x + i * 42 + 4, 144, 28, 14, C_GRY); }
            ftext_draw_center(FACE_GENEVA, x + i * 42 + 18, 142, i == sc ? "\xFB" : "", C_WHT, -1);
            (void)cshade;
        }
        ksnprintf(b, sizeof b, "Color: %s", ccol[sc]); ftext_draw(FACE_GENEVA, x, 166, b, C_DGR, -1);
        ftext_draw(FACE_GENEVA | FS_BOLD, x, 190, "Date & Time", C_BLK, -1);
        static const int fx[5] = { 0, 40, 80, 150, 190 };
        for (int i = 0; i < 5; i++) {
            ksnprintf(b, sizeof b, i == 2 ? "%04d" : "%02d", tf[i]);
            ftext_draw(FACE_GENEVA, x + fx[i], 212, b, C_BLK, -1);
            ui_arrow(x + fx[i], 230, 1); ui_arrow(x + fx[i] + 20, 230, 0);
        }
        static const char *const fl[5] = { "day", "month", "year", "hour", "min" };
        for (int i = 0; i < 5; i++) ftext_draw(FACE_GENEVA, x + fx[i], 250, fl[i], C_DGR, -1);
        widget_button(x + 250, 208, 50, 24, "Set", 0);
    } else if (pane == P_MOUSE) {
        static const char *const sp[4] = { "Slow", "Medium", "Fast", "Faster" }, *const dc[3] = { "Slow", "Medium", "Fast" };
        ftext_draw(FACE_GENEVA | FS_BOLD, x, 8, "Mouse Tracking", C_BLK, -1); radios(x, 30, sp, 4, gos_setting_get(SET_MOUSE_SPEED));
        ftext_draw(FACE_GENEVA | FS_BOLD, x, 130, "Double-Click Speed", C_BLK, -1); radios(x, 152, dc, 3, gos_setting_get(SET_DBLCLICK));
        draw_box(x + 130, 150, 120, 60, dbl_hit ? C_GRN : C_WHT, C_BLK);
        ftext_draw_center(FACE_GENEVA, x + 190, 172, dbl_hit ? "OK!" : "Double-click here", C_BLK, -1);
    } else if (pane == P_KBD) {
        static const char *const rt[4] = { "Slow", "Medium", "Fast", "Fastest" }, *const dl[4] = { "Long", "Medium", "Short", "Shortest" };
        ftext_draw(FACE_GENEVA | FS_BOLD, x, 8, "Key Repeat Rate", C_BLK, -1); radios(x, 30, rt, 4, gos_setting_get(SET_KEY_RATE));
        ftext_draw(FACE_GENEVA | FS_BOLD, x, 130, "Delay Until Repeat", C_BLK, -1); radios(x, 152, dl, 4, gos_setting_get(SET_KEY_DELAY));
        draw_box(x + 130, 30, w->cw - x - 140, 90, C_WHT, C_BLK);
        ftext_draw(FACE_GENEVA, x + 136, 36, "Hold down a key:", C_DGR, -1);
        ftext_draw(FACE_GENEVA, x + 136, 60, kbd_text, C_BLK, -1);
    } else {
        int v = gos_setting_get(SET_VOLUME);
        ftext_draw(FACE_GENEVA | FS_BOLD, x, 8, "Speaker Volume", C_BLK, -1);
        for (int i = 0; i < 8; i++) { draw_box(x + i * 26, 100 - i * 8 - 8, 22, 8 + i * 8, i < v ? C_BLK : C_WHT, C_BLK); }
        ksnprintf(b, sizeof b, v ? "Volume %d" : "Volume 0 (silent: menu bar flashes)", v); ftext_draw(FACE_GENEVA, x, 112, b, C_BLK, -1);
        widget_button(x, 140, 110, 26, "Test Beep", 0);
        ftext_draw(FACE_GENEVA, x, 180, "PC speaker: volume is only", C_DGR, -1); ftext_draw(FACE_GENEVA, x, 196, "on/off (0 = silent).", C_DGR, -1);
    }
    win_present(w);
}

static int hit_radio(int x, int y, int ex, int ey, int n) { for (int i = 0; i < n; i++) if (ui_in(ex, ey, x, y + i * 22, 120, 18)) return i; return -1; }

static void click(Win *w, int ex, int ey) {
    (void)w;
    if (ex < 110) { int i = (ey - 4) / 52; if (i >= 0 && i < NP) { pane = i; if (i == P_GEN) load_time(); } return; }
    int x = 124, r;
    if (pane == P_GEN) {
        for (int i = 0; i < GOS_DESK_PATTERNS; i++) if (ui_in(ex, ey, x + (i % 4) * 62, 26 + (i / 4) * 44, 56, 38)) gos_setting_set(SET_DESK_PATTERN, i);
        for (int i = 0; i < GOS_DESK_COLORS; i++) if (ui_in(ex, ey, x + i * 42, 140, 36, 22)) gos_setting_set(SET_DESK_COLOR, i);
        static const int fx[5] = { 0, 40, 80, 150, 190 }, lo[5] = { 1, 1, 2000, 0, 0 }, hi[5] = { 31, 12, 2099, 23, 59 };
        for (int i = 0; i < 5; i++) {
            if (ui_in(ex, ey, x + fx[i], 230, 16, 14)) tf[i] = tf[i] >= hi[i] ? lo[i] : tf[i] + 1;
            if (ui_in(ex, ey, x + fx[i] + 20, 230, 16, 14)) tf[i] = tf[i] <= lo[i] ? hi[i] : tf[i] - 1;
        }
        if (ui_in(ex, ey, x + 250, 208, 50, 24)) apply_time();
    } else if (pane == P_MOUSE) {
        if ((r = hit_radio(x, 30, ex, ey, 4)) >= 0) gos_setting_set(SET_MOUSE_SPEED, r);
        if ((r = hit_radio(x, 152, ex, ey, 3)) >= 0) gos_setting_set(SET_DBLCLICK, r);
        if (ui_in(ex, ey, x + 130, 150, 120, 60)) { static u32 last; u32 now = gos_ticks() * 10; dbl_hit = (now - last) < (u32)gos_dblclick_ms(); last = now; }
    } else if (pane == P_KBD) {
        if ((r = hit_radio(x, 30, ex, ey, 4)) >= 0) gos_setting_set(SET_KEY_RATE, r);
        if ((r = hit_radio(x, 152, ex, ey, 4)) >= 0) gos_setting_set(SET_KEY_DELAY, r);
    } else {
        for (int i = 0; i < 8; i++) if (ui_in(ex, ey, x + i * 26, 100 - i * 8 - 8, 22, 8 + i * 8)) { gos_setting_set(SET_VOLUME, i + 1); gos_beep(880, 100); }
        if (ui_in(ex, ey, x, 100, 200, 14)) gos_setting_set(SET_VOLUME, 0);
        if (ui_in(ex, ey, x, 140, 110, 26)) gos_beep(0, 0);
    }
}

int main(void) {
    static Win w; GEvent e; int dirty = 1, quit = 0;
    if (win_create(&w, "Control Panel", -1, 0, 450, 310) < 0) return 1;
    win_set_minsize(&w, 450, 310);
    load_time();
    while (!quit) {
        if (dirty) { draw(&w); dirty = 0; win_set_info(&w, pname[pane]); }
        if (win_next(&w, &e, -1) <= 0) break;
        do {
            switch (e.type) {
            case GE_MOUSE_DOWN: click(&w, e.a, e.b); dirty = 1; break;
            case GE_KEY:
                if (pane != P_KBD) break;
                if (e.a >= 32 && e.a < 127) { if (kbd_n > 30) kbd_n = 0; kbd_text[kbd_n++] = (char)e.a; kbd_text[kbd_n] = 0; dirty = 1; }
                else if (e.a == '\b' && kbd_n) { kbd_text[--kbd_n] = 0; dirty = 1; }
                break;
            case GE_RESIZE: dirty = 1; break;
            case GE_CLOSE: quit = 1; break;
            default: break;
            }
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    win_close(&w);
    return 0;
}
