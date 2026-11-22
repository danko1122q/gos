#include "gos.h"
#include "edbuf.h"

#define MAXFILE (256 * 1024)

enum { AK_NONE = 0, AK_SAVEAS, AK_FIND };

typedef struct {
    Win *w;
    EdBuf b;
    int cur, anchor;
    int top, left;
    int want;
    char path[128]; int warned;
    int asking; char pin[128]; int plen;
    char fstr[96];
    char note[40];
    int cols, rows;
    int dirty, follow, quit;
} Ed;

static int modified(Ed *e) { return !eb_is_clean(&e->b); }
static int has_sel(Ed *e) { return e->cur != e->anchor; }
static int sel_lo(Ed *e) { return e->cur < e->anchor ? e->cur : e->anchor; }
static int sel_hi(Ed *e) { return e->cur < e->anchor ? e->anchor : e->cur; }
static int line_start(Ed *e, int pos) { return e->b.ls[eb_line_of(&e->b, pos)]; }

static void clamp_sel(Ed *e) {
    if (e->cur < 0) e->cur = 0;
    if (e->cur > e->b.len) e->cur = e->b.len;
    if (e->anchor < 0) e->anchor = 0;
    if (e->anchor > e->b.len) e->anchor = e->b.len;
}

static void title(Ed *e) {
    const char *b = e->path[0] ? strrchr(e->path, '/') + 1 : "Untitled";
    char t[40]; ksnprintf(t, sizeof t, "%s%s", modified(e) ? "*" : "", b);
    win_set_title(e->w, t);
}
static void status(Ed *e) {
    int l = eb_line_of(&e->b, e->cur); char b[80], s[20] = "";
    if (has_sel(e)) ksnprintf(s, sizeof s, "  [%d selected]", sel_hi(e) - sel_lo(e));
    if (e->note[0]) ksnprintf(b, sizeof b, "%s", e->note);
    else ksnprintf(b, sizeof b, "Ln %d, Col %d  %d byte%s  %s", l + 1, e->cur - e->b.ls[l] + 1, e->b.len, s, e->path[0] ? e->path : "(not saved)");
    win_set_info(e->w, b);
}
static void refresh_ui(Ed *e) { title(e); status(e); e->dirty = 1; }

static void note(Ed *e, const char *s) { strlcpy(e->note, s, sizeof e->note); }

static int save(Ed *e) {
    if (!e->path[0]) return -1;
    int r = gfs_write(e->path, e->b.t ? e->b.t : "", (u32)e->b.len, 0);
    if (r < 0) { win_msgbox("Save failed", e->path, gos_strerror(r)); return r; }
    eb_mark_clean(&e->b); e->warned = 0; title(e); return 0;
}

static void ensure_visible(Ed *e) {
    int l = eb_line_of(&e->b, e->cur), c = e->cur - e->b.ls[l];
    if (e->rows < 1 || e->cols < 1) return;
    if (l < e->top) e->top = l;
    if (l >= e->top + e->rows) e->top = l - e->rows + 1;
    if (c < e->left) e->left = c;
    if (c >= e->left + e->cols) e->left = c - e->cols + 1;
}

static void ed_draw(Ed *e) {
    Win *w = e->w;
    win_begin(w);
    int cw = w->cw, ch = w->ch;
    int bar = e->asking ? 24 : 0;
    e->cols = (cw - 8) / 8; e->rows = (ch - bar) / 16;
    if (e->follow) { ensure_visible(e); e->follow = 0; }
    if (e->top >= e->b.nl) e->top = e->b.nl - 1;
    if (e->top < 0) e->top = 0;
    win_set_scroll(w, e->top, e->b.nl, e->rows > 0 ? e->rows : 1);
    draw_rect(0, 0, cw, ch, C_WHT);
    int cl = eb_line_of(&e->b, e->cur), s0 = sel_lo(e), s1 = sel_hi(e);
    for (int r = 0; r < e->rows; r++) {
        int l = e->top + r; if (l >= e->b.nl) break;
        int ls = e->b.ls[l], len = eb_line_len(&e->b, l);
        int n = len - e->left; if (n > e->cols) n = e->cols; if (n > 120) n = 120;
        char tmp[128];
        if (n > 0) {
            for (int i = 0; i < n; i++) { char c = e->b.t[ls + e->left + i]; tmp[i] = (c == '\t') ? ' ' : c; }
            tmp[n] = 0;
        }
        int has_nl = l + 1 < e->b.nl, line_end = ls + len + (has_nl ? 1 : 0);
        if (s1 > s0 && s0 < line_end && s1 > ls) {
            int a = (s0 > ls ? s0 : ls) - ls, z = (s1 < ls + len ? s1 : ls + len) - ls;
            int nlsel = (has_nl && s1 > ls + len) ? 1 : 0;
            int ca = a - e->left, cz = z - e->left + nlsel;
            if (ca < 0) ca = 0;
            if (cz > e->cols) cz = e->cols;
            if (cz > ca) draw_rect(4 + ca * 8, r * 16, (cz - ca) * 8, 16, C_BLU);
        }
        if (n > 0) text_draw(4, r * 16, tmp, C_BLK, -1);
        if (s1 > s0 && n > 0) {
            int a = (s0 > ls ? s0 : ls) - ls - e->left, z = (s1 < ls + len ? s1 : ls + len) - ls - e->left;
            if (a < 0) a = 0;
            if (z > n) z = n;
            if (s0 < ls + len && s1 > ls && z > a) {
                char sub[128]; memcpy(sub, tmp + a, (size_t)(z - a)); sub[z - a] = 0;
                text_draw(4 + a * 8, r * 16, sub, C_WHT, -1);
            }
        }
        int cc = e->cur - ls - e->left;
        if (l == cl && cc >= 0 && cc <= e->cols) draw_rect(4 + cc * 8, r * 16, 2, 16, C_RED);
    }
    if (bar) {
        draw_rect(0, ch - bar, cw, bar, C_WIN); draw_rect(0, ch - bar, cw, 1, C_BLK);
        char t[160]; ksnprintf(t, sizeof t, "%s%s_", e->asking == AK_FIND ? "Find: " : "Save as: ", e->pin);
        ftext_draw(FACE_GENEVA, 4, ch - bar + 4, t, C_BLK, -1);
    }
    win_present(w);
}

static void too_big(void) { win_msgbox("Editor", "Document too large.", "Limit is 256 KB."); }

static void sel_clear_to(Ed *e, int pos) { e->cur = e->anchor = pos; }

static void delete_sel(Ed *e) {
    if (!has_sel(e)) return;
    int lo = sel_lo(e), hi = sel_hi(e);
    eb_erase(&e->b, lo, hi - lo, e->cur);
    sel_clear_to(e, lo);
}

static void insert_text(Ed *e, const char *s, int n) {
    if (n <= 0) return;
    int sel = has_sel(e);
    if (sel) { eb_group_begin(&e->b); delete_sel(e); }
    if (eb_insert(&e->b, e->cur, s, n, e->cur) == 0) sel_clear_to(e, e->cur + n); else too_big();
    if (sel) eb_group_end(&e->b);
}

static void do_copy(Ed *e) {
    if (!has_sel(e)) return;
    int r = gos_clip_set(e->b.t + sel_lo(e), (u32)(sel_hi(e) - sel_lo(e)));
    if (r < 0) win_msgbox("Copy failed", gos_strerror(r), "");
    else note(e, "copied to clipboard");
}

static void do_cut(Ed *e) { if (has_sel(e)) { do_copy(e); delete_sel(e); } }

static void do_paste(Ed *e) {
    char *d; u32 l;
    if (gos_clip_text(&d, &l) < 0) return;
    char *o = d; int n = 0;
    for (u32 i = 0; i < l; i++) {
        char c = d[i];
        if ((c >= 32 && c < 127) || c == '\n' || c == '\t') o[n++] = c;
    }
    if (n) insert_text(e, o, n); else note(e, "clipboard empty");
    kfree(d);
}

static void do_undo(Ed *e, int redo) {
    int c = e->cur;
    if (redo ? eb_redo(&e->b, &c) : eb_undo(&e->b, &c)) { e->cur = e->anchor = c; clamp_sel(e); e->anchor = e->cur; }
    else note(e, redo ? "nothing to redo" : "nothing to undo");
}

static void select_all(Ed *e) { e->anchor = 0; e->cur = e->b.len; }

static void do_find(Ed *e, int dir) {
    if (!e->fstr[0]) return;
    int m = (int)strlen(e->fstr);
    int from = dir >= 0 ? e->cur : sel_lo(e) - 1;
    if (dir < 0 && from < 0) from = e->b.len;
    int p = eb_find(&e->b, e->fstr, from, dir, 1);
    if (p < 0) { note(e, "not found"); return; }
    e->anchor = p; e->cur = p + m;
    if (dir >= 0 ? p < from : p > from) note(e, "search wrapped"); 
}

static void ask(Ed *e, int kind, const char *prefill) {
    e->asking = kind; e->plen = 0; e->pin[0] = 0;
    if (prefill && prefill[0]) { strlcpy(e->pin, prefill, sizeof e->pin); e->plen = (int)strlen(e->pin); }
}

static void move_to(Ed *e, int np, int extend) {
    if (np < 0) np = 0;
    if (np > e->b.len) np = e->b.len;
    e->cur = np;
    if (!extend) e->anchor = np;
}

static int vline_pos(Ed *e, int dl) {
    int l = eb_line_of(&e->b, e->cur), c = e->cur - e->b.ls[l];
    if (e->want < c) e->want = c;
    int nl = l + dl;
    if (nl < 0) nl = 0;
    if (nl >= e->b.nl) nl = e->b.nl - 1;
    int n = eb_line_len(&e->b, nl), cc = e->want < n ? e->want : n;
    return e->b.ls[nl] + cc;
}

static void ed_key(Ed *e, int k, int mods) {
    e->dirty = 1; e->follow = 1; e->note[0] = 0;
    if (e->asking) {
        if (k == '\n' && e->plen) {
            int kind = e->asking; e->asking = 0;
            if (kind == AK_SAVEAS) {
                if (e->pin[0] != '/') path_join(e->pin, e->path, sizeof e->path);
                else strlcpy(e->path, e->pin, sizeof e->path);
                save(e);
            } else { strlcpy(e->fstr, e->pin, sizeof e->fstr); do_find(e, 1); }
            refresh_ui(e);
        } else if (k == 27) { e->asking = 0; }
        else if (k == '\b') { if (e->plen) e->pin[--e->plen] = 0; }
        else if (k >= 32 && k < 127 && e->plen < 90) { e->pin[e->plen++] = (char)k; e->pin[e->plen] = 0; }
        return;
    }
    int extend = (mods & MOD_SHIFT) != 0, keep_want = 0;
    if (mods & MOD_CTRL) {
        switch (k) {
        case 's': if (e->path[0]) save(e); else ask(e, AK_SAVEAS, NULL); break;
        case 'a': select_all(e); break;
        case 'c': case KEY_INS: do_copy(e); break;
        case 'x': do_cut(e); break;
        case 'v': do_paste(e); break;
        case 'z': do_undo(e, extend); break;
        case 'y': do_undo(e, 1); break;
        case 'f': ask(e, AK_FIND, e->fstr); break;
        case KEY_HOME: move_to(e, 0, extend); break;
        case KEY_END: move_to(e, e->b.len, extend); break;
        }
        e->want = e->cur - line_start(e, e->cur);
        refresh_ui(e); return;
    }
    switch (k) {
    case KEY_LEFT:  move_to(e, (!extend && has_sel(e)) ? sel_lo(e) : e->cur - 1, extend); break;
    case KEY_RIGHT: move_to(e, (!extend && has_sel(e)) ? sel_hi(e) : e->cur + 1, extend); break;
    case KEY_UP:   move_to(e, vline_pos(e, -1), extend); keep_want = 1; break;
    case KEY_DOWN: move_to(e, vline_pos(e, 1), extend); keep_want = 1; break;
    case KEY_PGUP: move_to(e, vline_pos(e, -(e->rows > 1 ? e->rows : 1)), extend); keep_want = 1; break;
    case KEY_PGDN: move_to(e, vline_pos(e, e->rows > 1 ? e->rows : 1), extend); keep_want = 1; break;
    case KEY_HOME: move_to(e, line_start(e, e->cur), extend); break;
    case KEY_END: { int l = eb_line_of(&e->b, e->cur); move_to(e, e->b.ls[l] + eb_line_len(&e->b, l), extend); break; }
    case '\b': if (has_sel(e)) delete_sel(e); else if (e->cur > 0) { eb_erase(&e->b, e->cur - 1, 1, e->cur); sel_clear_to(e, e->cur - 1); } break;
    case KEY_DEL:
        if (extend) do_cut(e);
        else if (has_sel(e)) delete_sel(e);
        else if (e->cur < e->b.len) { eb_erase(&e->b, e->cur, 1, e->cur); e->anchor = e->cur; }
        break;
    case KEY_INS: if (extend) do_paste(e); break;
    case KEY_F3: do_find(e, extend ? -1 : 1); break;
    case '\n': insert_text(e, "\n", 1); break;
    case '\t': insert_text(e, "    ", 4); break;
    default: if (k >= 32 && k < 127) { char c = (char)k; insert_text(e, &c, 1); }
    }
    if (!keep_want) e->want = e->cur - line_start(e, e->cur);
    clamp_sel(e);
    refresh_ui(e);
}

static int pos_at(Ed *e, int lx, int ly) {
    int row = ly < 0 ? -1 : ly / 16;
    int l = e->top + row; if (l < 0) l = 0; if (l >= e->b.nl) l = e->b.nl - 1;
    int c = lx < 4 ? e->left - 1 : (lx - 4) / 8 + e->left; if (c < 0) c = 0;
    int n = eb_line_len(&e->b, l); if (c > n) c = n;
    return e->b.ls[l] + c;
}

static void ed_mouse(Ed *e, int lx, int ly, int down) {
    if (e->asking && e->w->ch - ly <= 24) return;
    int p = pos_at(e, lx, ly);
    e->cur = p;
    if (down) e->anchor = p;
    e->want = p - line_start(e, p);
    e->follow = 1; e->note[0] = 0;
    status(e); e->dirty = 1;
}

static void ed_cmd(Ed *e, int menu, int idx) {
    e->dirty = 1; e->note[0] = 0; e->follow = 1;
    if (menu == 0) {
        if (idx == 0) { if (e->path[0]) save(e); else ask(e, AK_SAVEAS, NULL); }
        else if (idx == 1) ask(e, AK_SAVEAS, e->path);
        else if (idx == 2 && e->path[0]) {
            char *d; u32 l;
            if (!gfs_read(e->path, &d, &l)) {
                if (eb_load(&e->b, d, (int)l) == 0) { e->cur = e->anchor = 0; e->top = e->left = 0; } else too_big();
                kfree(d);
            }
        }
    } else if (menu == 1) {
        if (idx == 0) ask(e, AK_FIND, e->fstr);
        else if (idx == 1) do_find(e, 1);
        else if (idx == 2) do_find(e, -1);
    } else if (menu == 2) {
        switch (idx) {
        case 0: do_undo(e, 0); break;
        case 1: do_undo(e, 1); break;
        case 2: do_cut(e); break;
        case 3: do_copy(e); break;
        case 4: do_paste(e); break;
        case 5: select_all(e); break;
        }
    }
    clamp_sel(e);
    e->want = e->cur - line_start(e, e->cur);
    refresh_ui(e);
}

int main(int argc, char **argv) {
    static Win w; static Ed e;
    if (win_create(&w, "Untitled", -1, 0, 480, 340) < 0) return 1;
    win_set_menu(&w, "Save  (Ctrl+S)\nSave As...\nRevert", "Find...  (Ctrl+F)\nFind Next  (F3)\nFind Previous  (Shift+F3)");
    win_set_edit_menu(&w, "Undo  (Ctrl+Z)\nRedo  (Ctrl+Y)\nCut  (Ctrl+X)\nCopy  (Ctrl+C)\nPaste  (Ctrl+V)\nSelect All  (Ctrl+A)");
    e.w = &w; e.cols = 50; e.rows = 14; e.dirty = 1; e.follow = 1;
    eb_init(&e.b, MAXFILE);
    if (argc > 1 && argv[1][0]) {
        path_join(argv[1], e.path, sizeof e.path);
        char *d; u32 l;
        if (!gfs_read(e.path, &d, &l)) {
            if (eb_load(&e.b, d, (int)l) < 0) win_msgbox("Editor", "File too large for the editor.", "Limit is 256 KB.");
            kfree(d);
        }
    }
    refresh_ui(&e);

    GEvent ev;
    while (!e.quit) {
        if (e.dirty) { ed_draw(&e); e.dirty = 0; }
        int r = win_next(&w, &ev, -1);
        if (r < 0) break;
        do {
            switch (ev.type) {
            case GE_KEY:        ed_key(&e, ev.a, ev.b); break;
            case GE_MOUSE_DOWN: ed_mouse(&e, ev.a, ev.b, 1); break;
            case GE_MOUSE_MOVE: if (ev.c) ed_mouse(&e, ev.a, ev.b, 0); break;
            case GE_CMD:        ed_cmd(&e, ev.a, ev.b); break;
            case GE_SCROLL:     e.top = ev.a; e.dirty = 1; break;
            case GE_RESIZE:     e.dirty = 1; break;
            case GE_CLOSE:
                if (!modified(&e) || e.warned) e.quit = 1;
                else { e.warned = 1; win_msgbox("Not saved", "Your changes will be lost.", "Ctrl+S to save, or close again to discard."); }
                break;
            default: break;
            }
        } while (!e.quit && (r = win_next(&w, &ev, 0)) > 0);
        if (r < 0) break;
    }
    win_close(&w);
    eb_free(&e.b);
    return 0;
}
