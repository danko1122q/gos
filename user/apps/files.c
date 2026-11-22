#include "uikit.h"

#define ICON_W 84
#define ICON_H 58
#define SMALL_W 150
#define SMALL_H 20
#define ROW_H  18
#define DISK_K 1023

typedef struct { char name[GOS_NAME_MAX]; u8 type; u16 mode; u32 size, mtime; } Ent;
enum { V_ICON, V_SMALL, V_NAME, V_DATE, V_SIZE, V_KIND };
typedef enum { P_NONE, P_RENAME, P_FIND } PromptKind;

typedef struct {
    Win *w; char path[128];
    Ent *e; u8 *sel; int n, view, cw;
    PromptKind pk; char pin[GOS_NAME_MAX]; int plen;
    u32 gen, last_ms; int last_i, anchor;
    int sc_pos, sc_total, sc_view;
    int press_i, press_x, press_y, dragging, mq, mqx, mqy, mqx1, mqy1;
    char fstr[28], froot[128]; int fk;
    int dirty, quit;
} Fm;

static int g_view;

static const char *kind_of(const Ent *e) {
    const char *d = strrchr(e->name, '.');
    if (e->type == VN_DIR) return "folder";
    if (d && !strcmp(d, ".app")) return "application";
    if (d && !strcmp(d, ".sh")) return "script";
    if (d && (!strcmp(d, ".txt") || !strcmp(d, ".md"))) return "text";
    if (d && (!strcmp(d, ".c") || !strcmp(d, ".h"))) return "source";
    return "document";
}
static int icon_of(const Ent *e) {
    const char *d = strrchr(e->name, '.');
    if (e->type == VN_DIR) return ICON_FOLDER;
    if (d && !strcmp(d, ".app")) return ICON_APP;
    if (d && !strcmp(d, ".sh")) return ICON_PROGRAM;
    if (d && (!strcmp(d, ".txt") || !strcmp(d, ".c") || !strcmp(d, ".h") || !strcmp(d, ".md"))) return ICON_TEXT;
    return ICON_FILE;
}
static int cmp(const void *pa, const void *pb) {
    const Ent *a = pa, *b = pb;
    int r = 0;
    if (g_view == V_DATE) r = a->mtime < b->mtime ? 1 : a->mtime > b->mtime ? -1 : 0;
    else if (g_view == V_SIZE) r = a->size < b->size ? 1 : a->size > b->size ? -1 : 0;
    else if (g_view == V_KIND) r = strcmp(kind_of(a), kind_of(b));
    else if ((a->type == VN_DIR) != (b->type == VN_DIR)) return a->type == VN_DIR ? -1 : 1;
    return r ? r : strcasecmp(a->name, b->name);
}

static int in_trash(Fm *f) { return !strcmp(f->path, GOS_TRASH_DIR); }
static int list_view(Fm *f) { return f->view >= V_NAME; }

static void fullpath(Fm *f, const char *name, char *out, int max) {
    if (!strcmp(f->path, "/")) ksnprintf(out, max, "/%s", name); else ksnprintf(out, max, "%s/%s", f->path, name);
}
static int nsel(Fm *f) { int c = 0; for (int i = 0; i < f->n; i++) c += f->sel[i]; return c; }
static int first_sel(Fm *f) { for (int i = 0; i < f->n; i++) if (f->sel[i]) return i; return -1; }
static void sel_only(Fm *f, int i) { memset(f->sel, 0, (size_t)(f->n ? f->n : 1)); if (i >= 0 && i < f->n) { f->sel[i] = 1; f->anchor = i; } }

static int percol(Fm *f) { int c = f->cw / (f->view == V_ICON ? ICON_W : SMALL_W); return c < 1 ? 1 : c; }
static int cell_h(Fm *f) { return f->view == V_ICON ? ICON_H : f->view == V_SMALL ? SMALL_H : ROW_H; }
static int cell_w(Fm *f) { return f->view == V_ICON ? ICON_W : f->view == V_SMALL ? SMALL_W : f->cw; }
static void cell(Fm *f, int i, int *x, int *y) {
    int pc = list_view(f) ? 1 : percol(f);
    *x = (i % pc) * cell_w(f); *y = (i / pc - f->sc_pos) * cell_h(f) + (f->view == V_ICON ? 4 : 0);
}
static int item_at(Fm *f, int lx, int ly) {
    int pc = list_view(f) ? 1 : percol(f), c = lx / cell_w(f), r = (ly - (f->view == V_ICON ? 4 : 0)) / cell_h(f) + f->sc_pos;
    if (ly < 0 || c >= pc) return -1;
    int i = r * pc + c;
    return (i >= 0 && i < f->n) ? i : -1;
}
static void sc_clamp(Fm *f) {
    int mx = f->sc_total - f->sc_view; if (mx < 0) mx = 0;
    if (f->sc_pos > mx) f->sc_pos = mx;
    if (f->sc_pos < 0) f->sc_pos = 0;
}

static void title_info(Fm *f) {
    const char *s = strrchr(f->path, '/');
    win_set_title(f->w, !strcmp(f->path, "/") ? "GOS HD" : in_trash(f) ? "Trash" : s ? s + 1 : f->path);
    GSysInfo si; gos_sysinfo(&si);
    u32 used = (si.vfs_bytes + 1023) / 1024; char b[80];
    ksnprintf(b, sizeof b, "%d items, %uK on disk, %uK available", f->n, used, used < DISK_K ? DISK_K - used : 0);
    win_set_info(f->w, b); f->gen = si.vfs_gen;
}

static void reload(Fm *f) {
    char keep[GOS_NAME_MAX] = ""; int k = first_sel(f);
    if (k >= 0) strlcpy(keep, f->e[k].name, sizeof keep);
    kfree(f->e); kfree(f->sel); f->e = NULL; f->sel = NULL; f->n = 0;
    if (!gfs_isdir(f->path)) strlcpy(f->path, "/", sizeof f->path);
    int c = 0; GDirEnt de;
    while (gos_readdir(f->path, c, &de) > 0) c++;
    f->e = kzalloc((size_t)(c ? c : 1) * sizeof(Ent)); f->sel = kzalloc((size_t)(c ? c : 1));
    if (!f->e || !f->sel) return;
    for (int i = 0; i < c; i++) {
        if (gos_readdir(f->path, i, &de) <= 0) break;
        if (de.name[0] == '.') continue;
        Ent *x = &f->e[f->n++];
        strlcpy(x->name, de.name, sizeof x->name); x->type = de.type; x->size = de.size;
        char p[200]; GStat st; fullpath(f, de.name, p, sizeof p);
        if (gos_stat(p, &st) == 0) { x->mode = st.mode; x->mtime = st.mtime; }
    }
    g_view = f->view; qsort(f->e, (size_t)f->n, sizeof(Ent), cmp);
    for (int i = 0; i < f->n; i++) if (keep[0] && !strcmp(f->e[i].name, keep)) { f->sel[i] = 1; f->anchor = i; }
    title_info(f); f->dirty = 1;
}

static void go(Fm *f, const char *path) {
    char abs[160]; path_join(path, abs, sizeof abs);
    if (!gfs_isdir(abs)) return;
    strlcpy(f->path, abs, sizeof f->path);
    f->sc_pos = 0; kfree(f->sel); f->sel = NULL; f->n = 0; reload(f);
}
static void parent(Fm *f) {
    char p[128]; strlcpy(p, f->path, sizeof p);
    char *s = strrchr(p, '/'); if (!s) return;
    if (s == p) p[1] = 0; else *s = 0;
    go(f, p);
}
static void select_name(Fm *f, const char *name) {
    for (int i = 0; i < f->n; i++) if (!strcmp(f->e[i].name, name)) { sel_only(f, i); f->dirty = 1; return; }
}

static void fm_draw(Fm *f) {
    Win *w = f->w;
    win_begin(w);
    int cw = w->cw, ch = w->ch; f->cw = cw;
    int bar = f->pk != P_NONE ? 24 : 0, vh = ch - bar;
    draw_rect(0, 0, cw, ch, C_WIN);
    int ch_ = cell_h(f), pc = list_view(f) ? 1 : percol(f);
    int rows = (f->n + pc - 1) / pc, view = (vh - (f->view == V_ICON ? 4 : 0)) / ch_;
    f->sc_total = rows; f->sc_view = view > 0 ? view : 1; sc_clamp(f);
    for (int i = 0; i < f->n; i++) {
        int x, y; cell(f, i, &x, &y);
        if (y + ch_ < 0 || y >= vh) continue;
        const Ent *e = &f->e[i]; int on = f->sel[i];
        if (f->view == V_ICON) {
            icon_draw_sel(x + (ICON_W - 32) / 2, y, icon_of(e), on);
            char t[32]; ftext_fit(FACE_GENEVA, e->name, ICON_W - 6, t, sizeof t);
            int tw = ftext_width(FACE_GENEVA, t);
            if (on) draw_rect(x + ICON_W / 2 - tw / 2 - 2, y + 34, tw + 4, 16, C_BLK);
            ftext_draw_center(FACE_GENEVA, x + ICON_W / 2, y + 34, t, on ? C_WHT : C_BLK, -1);
        } else {
            if (on) draw_rect(x, y, f->view == V_SMALL ? SMALL_W - 2 : cw, ch_, C_BLK);
            u8 fg = on ? C_WHT : C_BLK;
            icon_draw_small_sel(x + 4, y + (ch_ - 14) / 2, icon_of(e), on);
            char t[40];
            int nameend = f->view == V_SMALL ? SMALL_W - 2 : cw > 340 ? 150 : cw > 230 ? cw - 70 : cw - 4;
            ftext_fit(FACE_GENEVA, e->name, nameend - 22 - 6, t, sizeof t);
            ftext_draw(FACE_GENEVA, x + 22, y + (ch_ - 16) / 2, t, fg, -1);
            if (list_view(f)) {
                char b[32];
                if (cw > 230) { if (e->type == VN_DIR) strlcpy(b, "--", sizeof b); else ksnprintf(b, sizeof b, "%uK", (e->size + 1023) / 1024); ftext_draw(FACE_GENEVA, cw > 340 ? 150 : cw - 70, y + 1, b, fg, -1); }
                if (cw > 340) ftext_draw(FACE_GENEVA, 214, y + 1, kind_of(e), fg, -1);
                if (cw > 450) { DateTime d; epoch_to_dt(e->mtime, &d); ksnprintf(b, sizeof b, "%02d/%02d/%02d %02d:%02d", d.day, d.mon, d.year % 100, d.hour, d.min); ftext_draw(FACE_GENEVA, 300, y + 1, b, fg, -1); }
            }
        }
    }
    if (f->mq) {
        int x0 = MIN(f->mqx, f->mqx1), x1 = MAX(f->mqx, f->mqx1), y0 = MIN(f->mqy, f->mqy1), y1 = MAX(f->mqy, f->mqy1);
        draw_frame(x0, y0, x1 - x0 + 1, y1 - y0 + 1, C_BLK);
    }
    win_set_scroll(w, f->sc_pos, f->sc_total, f->sc_view);
    if (bar) {
        draw_rect(0, ch - bar, cw, bar, C_WHT); draw_rect(0, ch - bar, cw, 1, C_BLK);
        char t[80]; ksnprintf(t, sizeof t, "%s%s_", f->pk == P_FIND ? "Find: " : "New name: ", f->pin);
        ftext_draw(FACE_GENEVA, 4, ch - bar + 4, t, C_BLK, -1);
    }
    win_present(w);
}

static void err(const char *what, int r) { win_msgbox(what, r == -EEXIST ? "An item with that name already exists there." : r == -EACCES || r == -EPERM ? "You don't have permission for this item." : gos_strerror(r), ""); }

static void open_sel(Fm *f) {
    int c = nsel(f);
    for (int i = 0; i < f->n; i++) {
        if (!f->sel[i]) continue;
        char p[160]; fullpath(f, f->e[i].name, p, sizeof p);
        if (f->e[i].type == VN_DIR && c == 1) { go(f, p); return; }
        gos_launch(p);
    }
}
static void info_sel(Fm *f) {
    int c = 0;
    for (int i = 0; i < f->n; i++) if (f->sel[i]) { char p[160]; fullpath(f, f->e[i].name, p, sizeof p); const char *av[3] = { "getinfo", p, NULL }; gos_spawn("/bin/getinfo", av); c++; }
    if (!c) { const char *av[3] = { "getinfo", f->path, NULL }; gos_spawn("/bin/getinfo", av); }
}
static void each(Fm *f, int (*op)(const char *), const char *what) {
    for (int i = 0; i < f->n; i++) if (f->sel[i]) { char p[160]; fullpath(f, f->e[i].name, p, sizeof p); int r = op(p); if (r < 0) { err(what, r); break; } }
    memset(f->sel, 0, (size_t)(f->n ? f->n : 1)); reload(f);
}
static void trash_sel(Fm *f) {
    if (in_trash(f)) { win_msgbox("Trash", "That item is already in the Trash.", "Use Special > Empty Trash to delete it."); return; }
    if (nsel(f)) each(f, gos_trash, "Move to Trash");
}
static void new_folder(Fm *f) {
    char nm[GOS_NAME_MAX], p[160];
    for (int i = 1; i < 100; i++) { if (i == 1) strlcpy(nm, "untitled folder", sizeof nm); else ksnprintf(nm, sizeof nm, "untitled folder %d", i); fullpath(f, nm, p, sizeof p); if (!gfs_exists(p)) break; }
    int r = gos_mkdir(p);
    if (r < 0) { err("New Folder", r); return; }
    reload(f); select_name(f, nm);
    f->pk = P_RENAME; strlcpy(f->pin, nm, sizeof f->pin); f->plen = (int)strlen(nm);
}

static char found[64][128]; static int nfound;
static int ci_has(const char *s, const char *p) {
    size_t n = strlen(p);
    for (; *s; s++) if (!strncasecmp(s, p, n)) return 1;
    return 0;
}
static void collect(const char *dir, const char *pat, int depth, int *visited) {
    GDirEnt de;
    for (int i = 0; nfound < 64 && *visited < 3000 && gos_readdir(dir, i, &de) > 0; i++) {
        (*visited)++;
        char q[128]; ksnprintf(q, sizeof q, "%s/%s", !strcmp(dir, "/") ? "" : dir, de.name);
        if (de.name[0] == '.') continue;
        if (ci_has(de.name, pat) && nfound < 64) strlcpy(found[nfound++], q, sizeof found[0]);
        if (de.type == VN_DIR && depth < 8 && strcmp(q, "/proc") && strcmp(q, "/dev")) collect(q, pat, depth + 1, visited);
    }
}
static void do_find(Fm *f, int again) {
    if (!again) { strlcpy(f->fstr, f->pin, sizeof f->fstr); strlcpy(f->froot, f->path, sizeof f->froot); f->fk = 0; }
    if (!f->fstr[0]) return;
    int visited = 0; nfound = 0;
    collect(f->froot[0] ? f->froot : f->path, f->fstr, 0, &visited);
    if (!nfound) { win_msgbox("Find", "Not found.", f->fstr); return; }
    if (again) f->fk++;
    if (f->fk >= nfound) f->fk = 0;
    char *m = found[f->fk], dir[128]; strlcpy(dir, m, sizeof dir);
    char *s = strrchr(dir, '/'); const char *nm = s + 1;
    if (s == dir) dir[1] = 0; else *s = 0;
    char nmc[GOS_NAME_MAX]; strlcpy(nmc, nm, sizeof nmc);
    go(f, dir); select_name(f, nmc);
}

static void finish_prompt(Fm *f, int ok) {
    PromptKind k = f->pk; f->pk = P_NONE; f->dirty = 1;
    if (!ok || !f->plen) return;
    if (k == P_FIND) { do_find(f, 0); return; }
    int i = first_sel(f); if (i < 0) return;
    char p[160], q[160]; fullpath(f, f->e[i].name, p, sizeof p); fullpath(f, f->pin, q, sizeof q);
    if (strcmp(f->e[i].name, f->pin)) { int r = gos_rename(p, q); if (r < 0) err("Rename", r); }
    reload(f); select_name(f, f->pin);
}
static void start_prompt(Fm *f, PromptKind k) {
    if (k == P_RENAME) { int i = first_sel(f); if (i < 0) return; strlcpy(f->pin, f->e[i].name, sizeof f->pin); }
    else f->pin[0] = 0;
    f->pk = k; f->plen = (int)strlen(f->pin); f->dirty = 1;
}

static void on_drop(Fm *f, int lx, int ly) {
    char src[128]; if (gos_drag_get(src, sizeof src) <= 0) return;
    const char *base = strrchr(src, '/'); base = base ? base + 1 : src;
    int i = item_at(f, lx, ly); char dir[160]; strlcpy(dir, f->path, sizeof dir);
    if (i >= 0 && f->e[i].type == VN_DIR && strcmp(f->e[i].name, base)) fullpath(f, f->e[i].name, dir, sizeof dir);
    char par[128]; strlcpy(par, src, sizeof par); { char *s = strrchr(par, '/'); if (s == par) par[1] = 0; else if (s) *s = 0; }
    if (!strcmp(par, dir)) return;
    int r;
    if (!strcmp(dir, GOS_TRASH_DIR)) r = gos_trash(src);
    else { char dst[200]; ksnprintf(dst, sizeof dst, "%s/%s", !strcmp(dir, "/") ? "" : dir, base); r = gfs_exists(dst) ? -EEXIST : gos_rename(src, dst); }
    if (r < 0) err("Move", r);
    reload(f);
}

static void ensure_visible(Fm *f, int i) {
    if (i < 0) return;
    int row = list_view(f) ? i : i / percol(f);
    if (row < f->sc_pos) f->sc_pos = row; else if (row >= f->sc_pos + f->sc_view) f->sc_pos = row - f->sc_view + 1;
}

static void fm_key(Fm *f, int k, int mods) {
    f->dirty = 1;
    if (f->pk != P_NONE) {
        if (k == '\n') finish_prompt(f, 1);
        else if (k == 27) finish_prompt(f, 0);
        else if (k == '\b') { if (f->plen) f->pin[--f->plen] = 0; }
        else if (k >= 32 && k < 127 && k != '/' && f->plen < GOS_NAME_MAX - 2) { f->pin[f->plen++] = (char)k; f->pin[f->plen] = 0; }
        return;
    }
    if (mods & MOD_CTRL) {
        switch (k) {
        case 'a': memset(f->sel, 1, (size_t)f->n); break;
        case 'i': info_sel(f); break;
        case 'd': each(f, gos_duplicate, "Duplicate"); break;
        case 'f': start_prompt(f, P_FIND); break;
        case 'g': do_find(f, 1); break;
        case 'n': new_folder(f); break;
        case 'o': open_sel(f); break;
        }
        return;
    }
    int pc = list_view(f) ? 1 : percol(f), s = first_sel(f), last = -1;
    for (int i = 0; i < f->n; i++) if (f->sel[i]) last = i;
    int cur = (k == KEY_LEFT || k == KEY_UP || k == KEY_HOME || k == KEY_PGUP) ? s : last, nx = cur;
    switch (k) {
    case KEY_RIGHT: nx = cur < 0 ? 0 : cur + 1; break;
    case KEY_LEFT:  nx = cur < 0 ? 0 : cur - 1; break;
    case KEY_DOWN:  nx = cur < 0 ? 0 : cur + pc; break;
    case KEY_UP:    nx = cur < 0 ? 0 : cur - pc; break;
    case KEY_HOME:  nx = 0; break;
    case KEY_END:   nx = f->n - 1; break;
    case KEY_PGDN:  f->sc_pos += f->sc_view; sc_clamp(f); return;
    case KEY_PGUP:  f->sc_pos -= f->sc_view; sc_clamp(f); return;
    case '\n': open_sel(f); return;
    case '\b': parent(f); return;
    case KEY_DEL: trash_sel(f); return;
    case KEY_F2: start_prompt(f, P_RENAME); return;
    case KEY_F5: reload(f); return;
    default: return;
    }
    if (nx >= f->n) nx = k == KEY_DOWN && cur < f->n - 1 ? f->n - 1 : cur;
    if (nx < 0) nx = 0;
    if (f->n) { sel_only(f, nx); ensure_visible(f, nx); }
}

static void fm_mouse(Fm *f, int ev, int lx, int ly, int mods) {
    f->dirty = 1;
    if (ev == GE_MOUSE_DOWN) {
        int i = item_at(f, lx, ly), multi = mods & (MOD_SHIFT | MOD_CTRL);
        f->press_i = i; f->press_x = lx; f->press_y = ly; f->dragging = 0; f->mq = 0;
        if (i < 0) { if (!multi) memset(f->sel, 0, (size_t)(f->n ? f->n : 1)); if (!list_view(f)) { f->mq = 1; f->mqx = f->mqx1 = lx; f->mqy = f->mqy1 = ly; } return; }
        u32 now = gos_ticks() * 10;
        if (!multi && i == f->last_i && f->sel[i] && now - f->last_ms < (u32)gos_dblclick_ms()) { open_sel(f); f->last_i = -1; f->press_i = -1; return; }
        if (multi) f->sel[i] = !f->sel[i]; else if (!f->sel[i]) sel_only(f, i);
        f->last_i = i; f->last_ms = now; f->anchor = i;
    } else if (ev == GE_MOUSE_MOVE) {
        if (f->mq) {
            f->mqx1 = lx; f->mqy1 = ly;
            int x0 = MIN(f->mqx, lx), x1 = MAX(f->mqx, lx), y0 = MIN(f->mqy, ly), y1 = MAX(f->mqy, ly);
            for (int i = 0; i < f->n; i++) { int cx, cy; cell(f, i, &cx, &cy); f->sel[i] = !(cx + cell_w(f) <= x0 || cx >= x1 || cy + cell_h(f) <= y0 || cy >= y1); }
        } else if (f->press_i >= 0 && !f->dragging && abs(lx - f->press_x) + abs(ly - f->press_y) > 4) {
            char p[160]; fullpath(f, f->e[f->press_i].name, p, sizeof p);
            f->dragging = 1;
            if (gos_drag_begin(p) < 0) f->press_i = -1;
        }
    } else if (ev == GE_MOUSE_UP) { f->mq = 0; f->press_i = -1; f->dragging = 0; }
}

static void fm_cmd(Fm *f, int menu, int idx) {
    f->dirty = 1;
    if (menu == 1) { f->view = idx; f->sc_pos = 0; reload(f); return; }
    if (menu == 2) { if (idx == 0) memset(f->sel, 1, (size_t)f->n); return; }
    switch (idx) {
    case 0: open_sel(f); break;
    case 1: new_folder(f); break;
    case 2: info_sel(f); break;
    case 3: start_prompt(f, P_RENAME); break;
    case 4: each(f, gos_duplicate, "Duplicate"); break;
    case 5: trash_sel(f); break;
    case 6: each(f, gos_putaway, "Put Away"); break;
    case 7: start_prompt(f, P_FIND); break;
    case 8: do_find(f, 1); break;
    case 9: parent(f); break;
    }
}

static void fm_tick(Fm *f) {
    GSysInfo si; gos_sysinfo(&si);
    if (si.vfs_gen != f->gen && f->pk == P_NONE && !f->dragging) reload(f);
}

int main(int argc, char **argv) {
    static Win w; static Fm f;
    if (win_create(&w, "Files", -1, 0, 480, 330) < 0) return 1;
    win_set_minsize(&w, 260, 150);
    win_set_menu(&w, "Open\nNew Folder\nGet Info\nRename\nDuplicate\nMove to Trash\nPut Away\nFind...\nFind Again\nParent Folder",
                 "by Icon\nby Small Icon\nby Name\nby Date\nby Size\nby Kind");
    win_set_edit_menu(&w, "Select All");
    f.w = &w; f.view = V_ICON; f.last_i = -1; f.press_i = -1; f.cw = 460;
    strlcpy(f.path, argc > 1 && argv[1][0] ? argv[1] : "/", sizeof f.path);
    reload(&f);

    GEvent e;
    while (!f.quit) {
        if (f.dirty) { fm_draw(&f); f.dirty = 0; }
        int r = win_next(&w, &e, 1000);
        if (r < 0) break;
        if (r == 0) { fm_tick(&f); continue; }
        do {
            switch (e.type) {
            case GE_KEY:        fm_key(&f, e.a, e.b); break;
            case GE_MOUSE_DOWN: case GE_MOUSE_MOVE: case GE_MOUSE_UP: fm_mouse(&f, e.type, e.a, e.b, e.d); break;
            case GE_DROP:       on_drop(&f, e.a, e.b); break;
            case GE_CMD:        fm_cmd(&f, e.a, e.b); break;
            case GE_SCROLL:     f.sc_pos = e.a; f.dirty = 1; break;
            case GE_RESIZE:     f.dirty = 1; break;
            case GE_CLOSE:      f.quit = 1; break;
            default: break;
            }
        } while (!f.quit && (r = win_next(&w, &e, 0)) > 0);
        if (r < 0) break;
        fm_tick(&f);
    }
    win_close(&w);
    kfree(f.e); kfree(f.sel);
    return 0;
}
