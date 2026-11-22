#include "gos.h"

static u8 fontbuf[4096];
static UiFont uifont[2];
static int font_ready;

static void font_load(void) {
    if (font_ready) return;
    if (gos_syscall(SYS_GETFONT, (int)fontbuf, 0, 0, 0, 0) == 0) font_set_base(fontbuf);
    for (int f = FACE_CHICAGO; f <= FACE_GENEVA; f++)
        if (gos_syscall(SYS_GETUIFONT, f, (int)&uifont[f - 1], 0, 0, 0) == 0) font_set_ui(f, &uifont[f - 1]);
    font_ready = 1;
}

static void refresh(Win *w) {
    GWinInfo gi;
    if (gos_syscall(SYS_WIN_INFO, w->id, (int)&gi, 0, 0, 0) == 0) { w->cw = gi.cw; w->ch = gi.ch; w->surf = (u8 *)gi.surf; }
}

int win_create(Win *w, const char *title, int x, int y, int ww, int wh) {
    font_load();
    int id = gos_syscall(SYS_WIN_CREATE, (int)title, x, y, ww, wh);
    if (id < 0) return id;
    w->id = id; w->surf = NULL; w->cw = w->ch = 0;
    refresh(w);
    if (!w->surf) return -EIO;
    return 0;
}

void win_begin(Win *w) {
    draw_set_target(w->surf);
    int cw = w->cw > GOS_SURF_W ? GOS_SURF_W : w->cw, ch = w->ch > GOS_SURF_H ? GOS_SURF_H : w->ch;
    draw_clip(0, 0, cw, ch);
}

int win_next(Win *w, GEvent *e, int timeout_ms) {
    int r = gos_syscall(SYS_WIN_EVENT, w->id, (int)e, timeout_ms, 0, 0);
    if (r > 0 && e->type == GE_RESIZE) { w->cw = e->a; w->ch = e->b; }
    return r;
}

void win_present(Win *w) { gos_syscall(SYS_WIN_PRESENT, w->id, 0, 0, 0, 0); }
void win_set_title(Win *w, const char *s) { gos_syscall(SYS_WIN_CTL, w->id, WC_TITLE, (int)s, 0, 0); }
void win_set_info(Win *w, const char *s)  { gos_syscall(SYS_WIN_CTL, w->id, WC_INFO, (int)s, 0, 0); }
void win_set_scroll(Win *w, int pos, int total, int view) { gos_syscall(SYS_WIN_CTL, w->id, WC_SCROLL, pos, total, view); }
void win_set_menu(Win *w, const char *f, const char *v) { gos_syscall(SYS_WIN_CTL, w->id, WC_MENU, (int)f, (int)v, 0); }
void win_set_edit_menu(Win *w, const char *items) { gos_syscall(SYS_WIN_CTL, w->id, WC_EDITMENU, (int)items, 0, 0); }
void win_set_minsize(Win *w, int mw, int mh) { gos_syscall(SYS_WIN_CTL, w->id, WC_MINSIZE, mw, mh, 0); }
void win_close(Win *w) { gos_syscall(SYS_WIN_CLOSE, w->id, 0, 0, 0, 0); }
void win_msgbox(const char *t, const char *l1, const char *l2) { gos_syscall(SYS_WIN_MSGBOX, (int)t, (int)l1, (int)l2, 0, 0); }
