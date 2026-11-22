#include "uikit.h"
#include "pagefile.h"

#define MAXC 48
#define MAXLEN 8000
#define FILE_ "/home/user/Documents/Scrapbook File"

static char *clip[MAXC]; static int n, cur, sc, dirty = 1, quit, changed;

static void load(void) {
    char *d; u32 l;
    if (gfs_read(FILE_, &d, &l) != 0) return;
    int off[MAXC], pl[MAXC], c = pf_split(d, (int)l, off, pl, MAXC);
    for (int i = 0; i < c; i++) {
        if (c == 1 && pl[0] == 0) break;
        clip[n] = kmalloc((size_t)pl[i] + 1); if (!clip[n]) break;
        memcpy(clip[n], d + off[i], (size_t)pl[i]); clip[n][pl[i]] = 0; n++;
    }
    kfree(d);
}
static void save(void) {
    u32 tot = 1; for (int i = 0; i < n; i++) tot += (u32)strlen(clip[i]) + 1;
    char *b = kmalloc(tot); if (!b) return; u32 o = 0;
    for (int i = 0; i < n; i++) { size_t l = strlen(clip[i]); memcpy(b + o, clip[i], l); o += (u32)l; if (i < n - 1) b[o++] = '\f'; }
    gos_mkdir("/home/user/Documents");
    gfs_write(FILE_, b, o, 0); kfree(b); changed = 0;
}
static void del_cur(void) { if (!n) return; kfree(clip[cur]); for (int i = cur; i < n - 1; i++) clip[i] = clip[i + 1]; n--; if (cur >= n) cur = n - 1; if (cur < 0) cur = 0; changed = 1; }
static void copy_cur(void) { if (n) gos_clip_set(clip[cur], (u32)strlen(clip[cur])); }
static void paste(void) {
    char *t; u32 l;
    if (n >= MAXC) { win_msgbox("Scrapbook", "The Scrapbook is full.", ""); return; }
    if (gos_clip_text(&t, &l) || !l) { kfree(t); gos_beep(0, 0); return; }
    if (l > MAXLEN) t[MAXLEN] = 0;
    int at = n ? cur + 1 : 0;
    for (int i = n; i > at; i--) clip[i] = clip[i - 1];
    clip[at] = t; n++; cur = at; changed = 1;
}

static void draw(Win *w) {
    win_begin(w); draw_rect(0, 0, w->cw, w->ch, C_WHT);
    char b[48]; ksnprintf(b, sizeof b, n ? "Clip %d of %d" : "Empty - Edit > Paste adds clipboard contents", cur + 1, n);
    win_set_info(w, b);
    if (n) {
        int cols = (w->cw - 8) / 8, rows = w->ch / 16, row = 0, col = 0; const char *t = clip[cur];
        for (int i = 0; t[i]; i++) {
            if (t[i] == '\n' || col >= cols) { row++; col = 0; if (t[i] == '\n') continue; }
            if (row - sc >= 0 && row - sc < rows && t[i] >= 32 && t[i] < 127) { char s[2] = { t[i], 0 }; text_draw(4 + col * 8, (row - sc) * 16, s, C_BLK, -1); }
            col++;
        }
    }
    win_set_scroll(w, cur, n > 0 ? n : 1, 1);
    win_present(w);
}

int main(void) {
    static Win w; GEvent e;
    if (win_create(&w, "Scrapbook", -1, 0, 360, 260) < 0) return 1;
    win_set_minsize(&w, 200, 120);
    win_set_edit_menu(&w, "Paste\nCopy\nCut\nClear");
    load();
    while (!quit) {
        if (dirty) { draw(&w); dirty = 0; }
        if (win_next(&w, &e, -1) <= 0) break;
        do {
            switch (e.type) {
            case GE_CMD:
                if (e.b == 0) paste(); else if (e.b == 1) copy_cur(); else if (e.b == 2) { copy_cur(); del_cur(); } else if (e.b == 3) del_cur();
                sc = 0; dirty = 1; if (changed) save(); break;
            case GE_SCROLL: cur = e.a < n ? e.a : n - 1; if (cur < 0) cur = 0; sc = 0; dirty = 1; break;
            case GE_KEY:
                if (e.a == KEY_PGDN || e.a == KEY_RIGHT) { if (cur < n - 1) cur++; sc = 0; }
                else if (e.a == KEY_PGUP || e.a == KEY_LEFT) { if (cur > 0) cur--; sc = 0; }
                else if (e.a == KEY_DEL) { del_cur(); save(); }
                else if (e.a == KEY_DOWN) sc++; else if (e.a == KEY_UP && sc > 0) sc--;
                dirty = 1; break;
            case GE_RESIZE: dirty = 1; break;
            case GE_CLOSE: quit = 1; break;
            default: break;
            }
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    if (changed) save();
    win_close(&w);
    return 0;
}
