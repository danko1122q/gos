#include "uikit.h"
#include "pagefile.h"

#define NP 8
#define MAXP 1800
#define FILE_ "/home/user/Documents/Note Pad File"

typedef struct { Win *w; char pg[NP][MAXP + 1]; int len[NP], cur, pos, want, cols, dirty, saved_dirty, quit; } Np;

static void load(Np *n) {
    char *d; u32 l;
    if (gfs_read(FILE_, &d, &l) != 0) return;
    int off[NP], pl[NP], c = pf_split(d, (int)l, off, pl, NP);
    for (int i = 0; i < c; i++) { int k = pl[i] > MAXP ? MAXP : pl[i]; memcpy(n->pg[i], d + off[i], (size_t)k); n->len[i] = k; n->pg[i][k] = 0; }
    kfree(d);
}
static void save(Np *n) {
    char *b = kmalloc((size_t)NP * (MAXP + 1)); if (!b) return;
    int o = 0;
    for (int i = 0; i < NP; i++) { memcpy(b + o, n->pg[i], (size_t)n->len[i]); o += n->len[i]; if (i < NP - 1) b[o++] = '\f'; }
    gos_mkdir("/home/user/Documents");
    gfs_write(FILE_, b, (u32)o, 0);
    kfree(b); n->saved_dirty = 0;
}

static void rc(Np *n, int idx, int *r, int *c) {
    const char *t = n->pg[n->cur]; int row = 0, col = 0;
    for (int i = 0; i < idx; i++) {
        if (t[i] == '\n' || col >= n->cols) { row++; col = 0; if (t[i] == '\n') continue; }
        col++;
    }
    if (col >= n->cols && idx < n->len[n->cur] && t[idx] != '\n') { row++; col = 0; }
    *r = row; *c = col;
}
static int idx_at(Np *n, int row, int want) {
    int best = -1, r, c;
    for (int i = 0; i <= n->len[n->cur]; i++) { rc(n, i, &r, &c); if (r == row && c <= want) best = i; if (r > row) break; }
    return best;
}

static void draw(Np *n) {
    Win *w = n->w; win_begin(w);
    draw_rect(0, 0, w->cw, w->ch, C_WHT);
    for (int y = 16; y < w->ch; y += 16) draw_rect(0, y - 1, w->cw, 1, C_LGR);
    n->cols = (w->cw - 8) / 8; if (n->cols < 4) n->cols = 4;
    const char *t = n->pg[n->cur]; int row = 0, col = 0;
    for (int i = 0; i <= n->len[n->cur]; i++) {
        if (i == n->pos) draw_rect(4 + col * 8, row * 16, 2, 16, C_BLK);
        if (i == n->len[n->cur]) break;
        if (t[i] == '\n' || col >= n->cols) { row++; col = 0; if (t[i] == '\n') { if (i + 1 == n->pos) draw_rect(4, row * 16, 2, 16, C_BLK); continue; } }
        if (t[i] >= 32 && t[i] < 127) { char s[2] = { t[i], 0 }; text_draw(4 + col * 8, row * 16, s, C_BLK, -1); }
        col++;
    }
    char b[40]; ksnprintf(b, sizeof b, "Page %d of %d", n->cur + 1, NP);
    win_set_info(w, b);
    win_set_scroll(w, n->cur, NP, 1);
    win_present(w);
}

static void go(Np *n, int p) { if (p < 0) p = 0; if (p >= NP) p = NP - 1; if (p != n->cur) { n->cur = p; n->pos = n->len[p]; } n->dirty = 1; }

static void key(Np *n, int k) {
    char *t = n->pg[n->cur]; int *L = &n->len[n->cur], r, c;
    n->dirty = 1;
    if ((k >= 32 && k < 127) || k == '\n') {
        if (*L >= MAXP) { gos_beep(0, 0); return; }
        memmove(t + n->pos + 1, t + n->pos, (size_t)(*L - n->pos + 1)); t[n->pos++] = (char)k; (*L)++; n->saved_dirty = 1; n->want = -1;
    } else if (k == '\b') { if (n->pos) { memmove(t + n->pos - 1, t + n->pos, (size_t)(*L - n->pos + 1)); n->pos--; (*L)--; n->saved_dirty = 1; } }
    else if (k == KEY_DEL) { if (n->pos < *L) { memmove(t + n->pos, t + n->pos + 1, (size_t)(*L - n->pos)); (*L)--; n->saved_dirty = 1; } }
    else if (k == KEY_LEFT) { if (n->pos) n->pos--; n->want = -1; }
    else if (k == KEY_RIGHT) { if (n->pos < *L) n->pos++; n->want = -1; }
    else if (k == KEY_HOME) { rc(n, n->pos, &r, &c); n->pos = idx_at(n, r, 0); n->want = -1; }
    else if (k == KEY_END) { rc(n, n->pos, &r, &c); int i = idx_at(n, r, n->cols); n->pos = i < 0 ? n->pos : i; n->want = -1; }
    else if (k == KEY_UP || k == KEY_DOWN) {
        rc(n, n->pos, &r, &c); if (n->want < 0) n->want = c;
        int i = idx_at(n, r + (k == KEY_UP ? -1 : 1), n->want); if (i >= 0) n->pos = i;
    }
    else if (k == KEY_PGUP) go(n, n->cur - 1);
    else if (k == KEY_PGDN) go(n, n->cur + 1);
}

int main(void) {
    static Win w; static Np n; GEvent e;
    if (win_create(&w, "Note Pad", -1, 0, 360, 260) < 0) return 1;
    win_set_minsize(&w, 200, 120);
    win_set_menu(&w, "Next Page\nPrevious Page\nDelete Page", "");
    n.w = &w; n.dirty = 1; n.want = -1; load(&n); n.pos = n.len[0];
    u32 last_save = gos_ticks();
    while (!n.quit) {
        if (n.dirty) { draw(&n); n.dirty = 0; }
        int r = win_next(&w, &e, 1000);
        if (r < 0) break;
        if (r == 0) { if (n.saved_dirty && gos_ticks() - last_save > 200) { save(&n); last_save = gos_ticks(); } continue; }
        do {
            switch (e.type) {
            case GE_KEY: key(&n, e.a); break;
            case GE_SCROLL: go(&n, e.a); break;
            case GE_CMD:
                if (e.b == 0) go(&n, n.cur + 1); else if (e.b == 1) go(&n, n.cur - 1);
                else if (e.b == 2) { n.len[n.cur] = 0; n.pg[n.cur][0] = 0; n.pos = 0; n.saved_dirty = 1; n.dirty = 1; }
                break;
            case GE_MOUSE_DOWN: {
                int row = e.b / 16, col = (e.a - 4 + 4) / 8, i = idx_at(&n, row, col); if (i >= 0) n.pos = i; n.want = -1; n.dirty = 1; break; }
            case GE_RESIZE: n.dirty = 1; break;
            case GE_CLOSE: n.quit = 1; break;
            default: break;
            }
        } while (!n.quit && win_next(&w, &e, 0) > 0);
    }
    if (n.saved_dirty) save(&n);
    win_close(&w);
    return 0;
}
