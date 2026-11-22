#include "shell.h"

#define TL_MAX   400
#define LINE_MAX 400
#define IN_MAX   200
#define HIST     16

typedef struct Term {
    Shell sh;
    char *lines[TL_MAX]; int nl;
    char in[IN_MAX]; int len, pos;
    char hist[HIST][IN_MAX]; int nh, hi;
    int follow, quit, dirty;
    int busy;
    char job[128]; int jl;
    char pend[1024]; int pend_n, pend_i;
    int sc_pos, sc_total, sc_view;
    Win *win;
} Term;

static void new_line(Term *t) {
    if (t->nl == TL_MAX) { kfree(t->lines[0]); memmove(t->lines, t->lines + 1, (TL_MAX - 1) * sizeof(char *)); t->nl--; }
    t->lines[t->nl] = kzalloc(2);
    if (t->lines[t->nl]) t->nl++;
}
static void put_ch(Term *t, char c) {
    char *l = t->lines[t->nl - 1];
    int n = (int)strlen(l);
    if (n >= LINE_MAX) return;
    char *nl = krealloc(l, n + 2);
    if (!nl) return;
    nl[n] = c; nl[n + 1] = 0;
    t->lines[t->nl - 1] = nl;
}
static void term_write(void *ui, const char *s, int n) {
    Term *t = ui;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\n') new_line(t);
        else if (c == '\r' || c == 27) continue;
        else if (c == '\t') { int col = (int)strlen(t->lines[t->nl - 1]); do put_ch(t, ' '); while (++col & 7); }
        else put_ch(t, c);
    }
    t->follow = 1; t->dirty = 1;
}
static void term_clear(void *ui) {
    Term *t = ui;
    for (int i = 0; i < t->nl; i++) kfree(t->lines[i]);
    t->nl = 0; new_line(t); t->follow = 1; t->dirty = 1;
}
static void term_exit(void *ui) { ((Term *)ui)->quit = 1; }

static void prompt_str(Term *t, char *b, int max) { shell_prompt(&t->sh, b, max); }

static void run_line(Term *t, const char *line) {
    char p[160]; prompt_str(t, p, sizeof p);
    term_write(t, p, (int)strlen(p));
    term_write(t, line, (int)strlen(line));
    term_write(t, "\n", 1);
    t->busy = 1; t->jl = 0;
    shell_exec(&t->sh, line);
    t->busy = 0; t->jl = 0; t->pend_n = t->pend_i = 0; t->dirty = 1;
    if (t->nl && t->lines[t->nl - 1][0]) new_line(t);
}

static int rows_of(int len, int cols) { return len <= cols ? 1 : (len + cols - 1) / cols; }

static void sc_clamp(Term *t) {
    int mx = t->sc_total - t->sc_view; if (mx < 0) mx = 0;
    if (t->sc_pos > mx) t->sc_pos = mx;
    if (t->sc_pos < 0) t->sc_pos = 0;
}

static void term_draw(Term *t, Win *w) {
    win_begin(w);
    int cw = w->cw, ch = w->ch;
    int cols = cw / 8 - 1, rows = ch / 16;
    if (cols < 8) cols = 8;
    if (rows < 1) rows = 1;
    char p[160]; prompt_str(t, p, sizeof p);
    char cur[LINE_MAX + IN_MAX + 8];
    int last = t->nl - 1, total = 0;
    int curpos;
    if (t->busy) {
        strlcpy(cur, last >= 0 ? t->lines[last] : "", sizeof cur);
        curpos = (int)strlen(cur);
    } else {
        ksnprintf(cur, sizeof cur, "%s%s", p, t->in);
        curpos = (int)strlen(p) + t->pos;
    }

    int reuse = last >= 0 && (t->busy || t->lines[last][0] == 0);
    for (int i = 0; i < t->nl - (reuse ? 1 : 0); i++) total += rows_of((int)strlen(t->lines[i]), cols);
    int in_first = total;
    total += rows_of((int)strlen(cur) + 1, cols);
    t->sc_total = total; t->sc_view = rows;
    if (t->follow) { t->sc_pos = total > rows ? total - rows : 0; t->follow = 0; }
    sc_clamp(t);
    win_set_scroll(w, t->sc_pos, total, rows);

    draw_rect(0, 0, cw, ch, C_BLK);
    int row = 0;
    for (int i = 0; i <= t->nl - (reuse ? 1 : 0); i++) {
        const char *s; int len;
        if (i == t->nl - (reuse ? 1 : 0)) { s = cur; len = (int)strlen(cur); }
        else { s = t->lines[i]; len = (int)strlen(s); }
        int nr = rows_of(len + (s == cur ? 1 : 0), cols);
        for (int r = 0; r < nr; r++, row++) {
            int sy = row - t->sc_pos;
            if (sy < 0 || sy >= rows) continue;
            char tmp[160]; int n = len - r * cols; if (n > cols) n = cols; if (n < 0) n = 0;
            if (n > 150) n = 150;
            memcpy(tmp, s + r * cols, n); tmp[n] = 0;
            text_draw(4, sy * 16, tmp, (s == cur) ? C_WHT : C_LGR, -1);
        }
    }
    int crow = in_first + curpos / cols - t->sc_pos, ccol = curpos % cols;
    if (crow >= 0 && crow < rows) {
        draw_rect(4 + ccol * 8, crow * 16, 8, 16, C_GRN);
        char c[2] = { cur[curpos], 0 };
        if (c[0]) text_draw(4 + ccol * 8, crow * 16, c, C_BLK, -1);
    }
    win_present(w);
}

static void set_input(Term *t, const char *s) { strlcpy(t->in, s, IN_MAX); t->len = (int)strlen(t->in); t->pos = t->len; }

static int is_copy_key(int k, int mods) {
    return ((mods & MOD_CTRL) && (mods & MOD_SHIFT) && k == 'c') ||
           ((mods & MOD_CTRL) && k == KEY_INS);
}

static int is_paste_key(int k, int mods) {
    return ((mods & MOD_CTRL) && k == 'v') ||
           ((mods & MOD_SHIFT) && k == KEY_INS);
}

static void term_copy(Term *t) {
    const char *src = t->busy ? t->job : t->in;
    int n = t->busy ? t->jl : t->len;
    if (n > 0) (void)gos_clip_set(src, (u32)n);
}

static void term_paste_line(Term *t) {
    char *src = NULL;
    u32 len = 0;
    if (gos_clip_text(&src, &len) < 0 || !src) return;
    if (t->busy) {
        u32 room = (u32)sizeof t->pend - (u32)t->pend_n;
        u32 take = len < room ? len : room;
        if (take) memcpy(t->pend + t->pend_n, src, take);
        t->pend_n += (int)take;
    } else {
        for (u32 i = 0; i < len && t->len < IN_MAX - 2; i++) {
            char c = src[i];
            if (c == '\r') continue;
            if (c == '\n') break;
            if (c == '\t') c = ' ';
            if ((unsigned char)c < 32 || (unsigned char)c > 126) continue;
            memmove(t->in + t->pos + 1, t->in + t->pos, (size_t)(t->len - t->pos + 1));
            t->in[t->pos++] = c;
            t->len++;
        }
    }
    kfree(src);
    t->dirty = 1;
}

static void term_key(Term *t, int k, int mods) {
    t->follow = 1; t->dirty = 1;
    if (is_copy_key(k, mods)) { term_copy(t); return; }
    if (is_paste_key(k, mods)) { term_paste_line(t); return; }
    if (k == KEY_PGUP) { t->sc_pos -= t->sc_view - 1; t->follow = 0; sc_clamp(t); return; }
    if (k == KEY_PGDN) { t->sc_pos += t->sc_view - 1; t->follow = 0; sc_clamp(t); return; }
    if (mods & MOD_CTRL) {
        if (k == 'l') term_clear(t);
        else if (k == 'c') { char p[160]; prompt_str(t, p, sizeof p); term_write(t, p, (int)strlen(p)); term_write(t, t->in, t->len); term_write(t, "^C\n", 3); set_input(t, ""); }
        else if (k == 'u') set_input(t, "");
        else if (k == 'a') t->pos = 0;
        else if (k == 'e') t->pos = t->len;
        else if (k == 'd' && !t->len) term_exit(t);
        return;
    }
    switch (k) {
    case '\n': {
        char line[IN_MAX]; strlcpy(line, t->in, sizeof line);
        set_input(t, "");
        if (line[0] && (!t->nh || strcmp(t->hist[t->nh - 1], line))) {
            if (t->nh == HIST) { memmove(t->hist[0], t->hist[1], (HIST - 1) * IN_MAX); t->nh--; }
            strlcpy(t->hist[t->nh++], line, IN_MAX);
        }
        t->hi = t->nh;
        run_line(t, line);
        break; }
    case '\b': if (t->pos > 0) { memmove(t->in + t->pos - 1, t->in + t->pos, t->len - t->pos + 1); t->pos--; t->len--; } break;
    case KEY_DEL: if (t->pos < t->len) { memmove(t->in + t->pos, t->in + t->pos + 1, t->len - t->pos); t->len--; } break;
    case KEY_LEFT: if (t->pos > 0) t->pos--; break;
    case KEY_RIGHT: if (t->pos < t->len) t->pos++; break;
    case KEY_HOME: t->pos = 0; break;
    case KEY_END: t->pos = t->len; break;
    case KEY_UP: if (t->hi > 0) set_input(t, t->hist[--t->hi]); break;
    case KEY_DOWN: if (t->hi < t->nh - 1) set_input(t, t->hist[++t->hi]); else { t->hi = t->nh; set_input(t, ""); } break;
    case '\t': {
        Buf list; buf_init(&list);
        int n = shell_complete(&t->sh, t->in, &t->len, IN_MAX, &list);
        t->pos = t->len;
        if (n > 1 && list.p) {
            char p[160]; prompt_str(t, p, sizeof p);
            term_write(t, p, (int)strlen(p)); term_write(t, t->in, t->len); term_write(t, "\n", 1);
            term_write(t, list.p, list.len);
            if (t->nl && t->lines[t->nl - 1][0]) new_line(t);
        }
        buf_free(&list);
        break; }
    default:
        if (k >= 32 && k < 127 && t->len < IN_MAX - 2) {
            memmove(t->in + t->pos + 1, t->in + t->pos, t->len - t->pos + 1);
            t->in[t->pos++] = (char)k; t->len++;
        }
    }
}

static void term_unput(Term *t) {
    char *l = t->lines[t->nl - 1];
    int n = (int)strlen(l);
    if (n > 0) l[n - 1] = 0;
}

static int job_key(Term *t, int k, char *out, int max) {
    if (k == '\n') {
        term_write(t, "\n", 1);
        int n = t->jl < max - 1 ? t->jl : max - 1;
        memcpy(out, t->job, (size_t)n);
        out[n++] = '\n';
        t->jl = 0;
        return n;
    }
    if (k == '\b') {
        if (t->jl > 0) {
            t->jl--;
            term_unput(t);
            t->dirty = 1;
        }
    } else if (k >= 32 && k < 127 && t->jl < (int)sizeof t->job - 2) {
        char c = (char)k;
        t->job[t->jl++] = c;
        term_write(t, &c, 1);
    }
    return 0;
}

static int job_feed_pending(Term *t, char *out, int max) {
    while (t->pend_i < t->pend_n) {
        int k = (unsigned char)t->pend[t->pend_i++];
        int r = job_key(t, k == '\r' ? 0 : k, out, max);
        if (r) {
            if (t->pend_i == t->pend_n) t->pend_i = t->pend_n = 0;
            return r;
        }
    }
    t->pend_i = t->pend_n = 0;
    return 0;
}

static int term_poll(void *ui, char *out, int max, int wait_ms) {
    Term *t = ui;
    if (!out || max < 2) return 0;
    int ret = job_feed_pending(t, out, max);
    if (ret) return ret;
    if (t->dirty) { term_draw(t, t->win); t->dirty = 0; }
    GEvent e;
    int r = win_next(t->win, &e, wait_ms);
    if (r < 0) { t->quit = 1; return SH_POLL_INT; }
    while (r > 0) {
        switch (e.type) {
        case GE_KEY: {
            int k = e.a;
            if (is_copy_key(k, e.b)) {
                term_copy(t);
            } else if (is_paste_key(k, e.b)) {
                term_paste_line(t);
                ret = job_feed_pending(t, out, max);
            } else if (k == KEY_PGUP) {
                t->sc_pos -= t->sc_view - 1; t->follow = 0; sc_clamp(t); t->dirty = 1;
            } else if (k == KEY_PGDN) {
                t->sc_pos += t->sc_view - 1; t->follow = 0; sc_clamp(t); t->dirty = 1;
            } else if (e.b & MOD_CTRL) {
                if (k == 'c') { term_write(t, "^C\n", 3); t->jl = 0; t->pend_n = t->pend_i = 0; return SH_POLL_INT; }
                if (k == 'd') { t->jl = 0; t->pend_n = t->pend_i = 0; return SH_POLL_EOF; }
            } else {
                ret = job_key(t, k, out, max);
            }
            break; }
        case GE_SCROLL: t->sc_pos = e.a; t->follow = 0; t->dirty = 1; break;
        case GE_RESIZE: t->dirty = 1; break;
        case GE_CLOSE:  t->quit = 1; t->pend_n = t->pend_i = 0; return SH_POLL_INT;
        default: break;
        }
        if (ret) break;
        r = win_next(t->win, &e, 0);
    }
    return ret;
}

static void term_tick(Term *t, Win *w) {
    char p[160]; prompt_str(t, p, sizeof p);
    char *c = strchr(p, ':');
    if (c) { int l = (int)strlen(p); if (l >= 2) p[l - 2] = 0; win_set_info(w, c + 1); }
}

int main(int argc, char **argv) {
    static Win w; static Term t;
    if (win_create(&w, "Terminal", -1, 0, 520, 340) < 0) return 1;
    win_set_minsize(&w, 240, 120);
    t.win = &w; t.follow = 1; t.dirty = 1;
    shell_init(&t.sh);
    t.sh.ui = &t; t.sh.ui_write = term_write; t.sh.ui_clear = term_clear; t.sh.ui_exit = term_exit; t.sh.ui_poll = term_poll;
    new_line(&t);
    shell_run_script(&t.sh, "/etc/profile");
    t.sh.status = 0;
    if (argc > 1 && argv[1][0]) run_line(&t, argv[1]);
    term_tick(&t, &w);

    GEvent e;
    while (!t.quit) {
        if (t.dirty) { term_draw(&t, &w); t.dirty = 0; }
        int r = win_next(&w, &e, 1000);
        if (r < 0) break;
        if (r == 0) { term_tick(&t, &w); continue; }
        do {
            switch (e.type) {
            case GE_KEY:    term_key(&t, e.a, e.b); break;
            case GE_SCROLL: t.sc_pos = e.a; t.follow = 0; t.dirty = 1; break;
            case GE_RESIZE: t.dirty = 1; break;
            case GE_CLOSE:  t.quit = 1; break;
            default: break;
            }
        } while (!t.quit && (r = win_next(&w, &e, 0)) > 0);
        if (r < 0) break;
        term_tick(&t, &w);
    }
    win_close(&w);
    return 0;
}
