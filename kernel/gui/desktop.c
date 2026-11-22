#include "gui/desktop.h"
#include "gui/window.h"
#include "gui/menu.h"
#include "gui/dialog.h"
#include "gui/metrics.h"
#include "gui/widget.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/icons.h"
#include "gfx/patterns.h"
#include "drivers/video.h"
#include "drivers/keyboard.h"
#include "drivers/speaker.h"
#include "arch/power.h"
#include "arch/io.h"
#include "arch/timer.h"
#include "proc/exec.h"
#include "fs/vfs.h"
#include "fs/fsops.h"
#include "fs/persist.h"
#include "sys/settings.h"
#include "proc/task.h"
#include "mem/heap.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/errno.h"

#define DESK_DIR   "/home/user/Desktop"
#define LAYOUT     "/etc/desktop.pos"
#define MAXDI      42
#define CELL_W     84
#define CELL_H     64

enum { DK_FILE = 0, DK_DISK, DK_TRASH };
typedef struct { char name[VFS_NAME]; int icon, x, y, kind; } DIcon;

static DIcon di[MAXDI];
static int ndi, desk_sel = -1, desk_focus;
static u32 di_gen = 0xFFFFFFFFu;

enum { D_NONE, D_MOVE, D_RESIZE, D_THUMB, D_CLIENT, D_ICON, D_DRAG };
static int mode = D_NONE, drag_id, dox, doy, grab, ox, oy;
static int mx, my, btn;
static volatile int dirty;
static char clk[6] = "00:00";
static u32 last_click_ms; static int last_click_di = -1;
static int flash;

static int drag_on, drag_icon, icon_press, press_x, press_y, drag_src_di = -1, drag_src_win;
static char drag_path[128];
static u16 drag_uid = GOS_UID_USER;

void desktop_invalidate(void) { dirty |= DIRTY_SCENE | DIRTY_CURSOR; }
int  desktop_focused(void) {
    static int seen_top;
    Window *t = wm_top(); int id = t ? t->id : 0;
    if (id != seen_top) { seen_top = id; if (id) desk_focus = 0; }
    return desk_focus || !t;
}
int  desktop_sel_kind(void) { return desk_sel < 0 || desk_sel >= ndi ? 0 : di[desk_sel].kind == DK_FILE ? 1 : 2; }
int  desktop_flash_active(void) { return flash > 0; }
void desktop_alert(void) { if (settings_get(SET_VOLUME)) speaker_beep(880, 120); else { flash = 2; dirty |= DIRTY_SCENE; } }

typedef struct { u16 u, g; } Cred;
static Cred cred_set(u16 uid) { Task *t = task_current(); Cred c = { t->uid, t->gid }; t->uid = uid; t->gid = uid; return c; }
static void cred_back(Cred c) { Task *t = task_current(); t->uid = c.u; t->gid = c.g; }

static void report(const char *what, int err) {
    char l[64];
    if (err == -EEXIST) ksnprintf(l, sizeof l, "An item with that name already exists there.");
    else if (err == -EACCES || err == -EPERM) ksnprintf(l, sizeof l, "You don't have permission for this item.");
    else if (err == -EINVAL) ksnprintf(l, sizeof l, "Invalid destination (e.g. into itself).");
    else ksnprintf(l, sizeof l, "Error code %d.", err);
    dialog_msg(what, l, "");
}

static const struct { const char *name; int r, g, b; } desk_col[DESK_COLORS] = {
    { "classic", 255, 255, 255 }, { "green", 0, 120, 72 }, { "blue", 0, 72, 140 }, { "grey", 96, 96, 96 },
    { "purple", 100, 60, 140 }, { "teal", 0, 110, 120 }
};
const char *desktop_color_name(int i) { return (i >= 0 && i < DESK_COLORS) ? desk_col[i].name : desk_col[0].name; }
int desktop_color_by_name(const char *n) {
    for (int i = 0; i < DESK_COLORS; i++) if (!strcmp(n, desk_col[i].name)) return i;
    return -1;
}
int desktop_color(void) { return settings_get(SET_DESK_COLOR); }
void desktop_apply_color(int i) {
    if (i < 0 || i >= DESK_COLORS) return;
    video_set_palette(C_DESK, desk_col[i].r, desk_col[i].g, desk_col[i].b);
    if (i == 0) video_set_palette(C_DESK2, 0, 0, 0);
    else video_set_palette(C_DESK2, desk_col[i].r * 5 / 9, desk_col[i].g * 5 / 9, desk_col[i].b * 5 / 9);
}

static int icon_for(VNode *n) {
    if (n->type == VN_DIR) return ICON_FOLDER;
    const char *e = strrchr(n->name, '.');
    if (e) {
        if (!strcmp(e, ".app")) {
            if (!strcmp(n->name, "Terminal.app")) return ICON_TERM;
            if (!strcmp(n->name, "Monitor.app")) return ICON_MONITOR;
            return ICON_APP;
        }
        if (!strcmp(e, ".sh")) return ICON_PROGRAM;
        if (!strcmp(e, ".txt") || !strcmp(e, ".md") || !strcmp(e, ".c") || !strcmp(e, ".h")) return ICON_TEXT;
    }
    return ICON_FILE;
}

static struct { char name[VFS_NAME]; int x, y; } lay[MAXDI * 2];
static int nlay;
static int lay_find(const char *name) { for (int i = 0; i < nlay; i++) if (!strcmp(lay[i].name, name)) return i; return -1; }
static void lay_load(void) {
    char *d; u32 l; (void)l; nlay = 0;
    if (vfs_read_file(NULL, LAYOUT, &d, &l)) return;
    for (char *p = d; *p && nlay < MAXDI * 2; ) {
        char *e = strchr(p, '\n'); if (e) *e = 0;
        int x = 0, y = 0; char *q = p;
        while (*q >= '0' && *q <= '9') x = x * 10 + (*q++ - '0');
        if (*q == ' ') q++;
        while (*q >= '0' && *q <= '9') y = y * 10 + (*q++ - '0');
        if (*q == ' ' && q[1]) { strlcpy(lay[nlay].name, q + 1, VFS_NAME); lay[nlay].x = x; lay[nlay].y = y; nlay++; }
        if (!e) break;
        p = e + 1;
    }
    kfree(d);
}
static void lay_save(void) {
    char *b = kmalloc((u32)(nlay * 48 + 1)); if (!b) return;
    int n = 0;
    for (int i = 0; i < nlay; i++) n += ksnprintf(b + n, 48, "%d %d %s\n", lay[i].x, lay[i].y, lay[i].name);
    Task *tk = task_current(); u16 ou = tk->uid, og = tk->gid;
    tk->uid = 0; tk->gid = 0;
    vfs_write_file(NULL, LAYOUT, b, (u32)n, 0);
    tk->uid = ou; tk->gid = og;
    kfree(b);
}
static void lay_set(const char *name, int x, int y) {
    int i = lay_find(name);
    if (i < 0) { if (nlay >= MAXDI * 2) return; i = nlay++; strlcpy(lay[i].name, name, VFS_NAME); }
    lay[i].x = x; lay[i].y = y; lay_save();
}

static int abs_i(int v) { return v < 0 ? -v : v; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void scan_icons(void) {
    char sel_name[VFS_NAME] = "";
    if (desk_sel >= 0 && desk_sel < ndi) strlcpy(sel_name, di[desk_sel].name, sizeof sel_name);
    u32 titems, tbytes; trash_info(&titems, &tbytes);
    ndi = 0;
    strlcpy(di[0].name, "GOS HD", VFS_NAME); di[0].icon = ICON_DRIVE; di[0].kind = DK_DISK; di[0].x = SCREEN_W - 52; di[0].y = GUI_MB + GUI_WIN0;
    strlcpy(di[1].name, "Trash", VFS_NAME);  di[1].icon = titems ? ICON_TRASH_FULL : ICON_TRASH; di[1].kind = DK_TRASH;
    di[1].x = SCREEN_W - 52; di[1].y = SCREEN_H - 70;
    ndi = 2;
    VNode *d = vfs_lookup(NULL, DESK_DIR);
    int per = (SCREEN_H - GUI_MB - 16) / CELL_H, slot = 0;
    if (d && d->type == VN_DIR)
        for (VNode *c = d->child; c && ndi < MAXDI; c = c->next) {
            strlcpy(di[ndi].name, c->name, sizeof di[0].name);
            di[ndi].icon = icon_for(c); di[ndi].kind = DK_FILE;
            di[ndi].x = 12 + (CELL_W - 32) / 2 + (slot / per) * CELL_W;
            di[ndi].y = GUI_MB + GUI_WIN0 + (slot % per) * CELL_H;
            slot++; ndi++;
        }
    for (int i = 0; i < ndi; i++) {
        int k = lay_find(di[i].name);
        if (k >= 0) { di[i].x = clampi(lay[k].x, 28, SCREEN_W - 60); di[i].y = clampi(lay[k].y, GUI_MB + 2, SCREEN_H - 56); }
    }
    di_gen = vfs_generation();
    desk_sel = -1;
    if (sel_name[0]) for (int i = 0; i < ndi; i++) if (!strcmp(di[i].name, sel_name)) desk_sel = i;
}

static int icon_at(int x, int y) {
    for (int i = ndi - 1; i >= 0; i--)
        if (x >= di[i].x - 24 && x < di[i].x + 56 && y >= di[i].y && y < di[i].y + 54) return i;
    return -1;
}

static int icon_path(int i, char *out, int max) {
    if (i < 0 || i >= ndi) return -1;
    if (di[i].kind == DK_DISK) { strlcpy(out, "/", (u32)max); return 0; }
    if (di[i].kind == DK_TRASH) { strlcpy(out, GOS_TRASH_DIR, (u32)max); return 0; }
    ksnprintf(out, max, "%s/%s", DESK_DIR, di[i].name);
    return 0;
}

static int is_folder_icon(int i) { return i >= 0 && i < ndi && di[i].kind == DK_FILE && di[i].icon == ICON_FOLDER; }

static void label(int i, int hot) {
    char t[32]; strlcpy(t, di[i].name, sizeof t);
    size_t nl = strlen(di[i].name);
    if (nl > 4 && !strcmp(di[i].name + nl - 4, ".app")) { strlcpy(t, di[i].name, nl - 3 < sizeof t ? nl - 3 : sizeof t); }
    char fit[32]; ftext_fit(FACE_GENEVA, t, 82, fit, sizeof fit);
    int cx = di[i].x + 16, len = ftext_width(FACE_GENEVA, fit);
    draw_rect(cx - len / 2 - 2, di[i].y + 36, len + 4, 16, hot ? C_BLK : C_WHT);
    ftext_draw_center(FACE_GENEVA, cx, di[i].y + 36, fit, hot ? C_WHT : C_BLK, -1);
}

static void screen_corners(void) {
    static const u8 cr[5] = { 5, 3, 2, 1, 1 };
    draw_noclip();
    for (int r = 0; r < 5; r++) {
        draw_rect(0, r, cr[r], 1, C_BLK);                  draw_rect(SCREEN_W - cr[r], r, cr[r], 1, C_BLK);
        draw_rect(0, SCREEN_H - 1 - r, cr[r], 1, C_BLK);   draw_rect(SCREEN_W - cr[r], SCREEN_H - 1 - r, cr[r], 1, C_BLK);
    }
}

void desktop_render(void) {
    draw_set_target(video_scene()); draw_noclip();
    draw_rect(0, GUI_MB, SCREEN_W, SCREEN_H - GUI_MB, C_DESK);
    int pat = settings_get(SET_DESK_PATTERN);
    if (pat > 0 && pat < GOS_DESK_PATTERNS) {
        u8 *buf = video_scene();
        for (int y = GUI_MB; y < SCREEN_H; y++) {
            u8 bits = desk_patterns[pat][y & 7]; u8 *row = buf + y * SCREEN_W;
            for (int x = 0; x < SCREEN_W; x++) if ((bits >> (7 - (x & 7))) & 1) row[x] = C_DESK2;
        }
    }
    int hover = drag_on ? icon_at(mx, my) : -1;
    int hover_ok = hover >= 0 && hover != drag_src_di && (di[hover].kind == DK_TRASH || is_folder_icon(hover));
    for (int i = 0; i < ndi; i++) {
        int on = desk_sel == i || (hover_ok && hover == i);
        icon_draw_sel(di[i].x, di[i].y, di[i].icon, on);
        label(i, on);
    }
    wm_draw_all();
    draw_noclip();
    if (drag_on && (abs_i(mx - press_x) + abs_i(my - press_y) > 4 || drag_src_di < 0)) {
        draw_frame(mx - 17, my - 17, 34, 34, C_WHT);
        icon_draw(mx - 16, my - 16, drag_icon);
    }
    menu_draw(mx, my, clk);
    dialog_draw();
    screen_corners();
}

static void launch_single(const char *prog) {
    for (int i = 0; i < GOS_MAXTASK; i++) {
        Task *t = task_get(i);
        if (!t || !t->pd || t->state == T_ZOMBIE || strcmp(t->name, prog)) continue;
        for (int k = 0; k < wm_count(); k++) { Window *w = wm_nth(k); if (w && w->owner == t) { wm_focus(w); desk_focus = 0; return; } }
    }
    launch_prog(prog, NULL);
}

static void open_icon(int i) {
    char p[160];
    if (icon_path(i, p, sizeof p)) return;
    if (di[i].kind != DK_FILE) { launch_prog("files", p); return; }
    app_open_path(p);
}

static void do_action(int a) {
    Window *t = wm_top();
    if (a >= A_FOCUS) { Window *w = wm_find(a - A_FOCUS); if (w) { wm_focus(w); desk_focus = 0; } return; }
    if (a >= A_WINCMD_EDIT) { if (t && t->cmd) t->cmd(t, 2, a - A_WINCMD_EDIT); return; }
    if (a >= A_WINCMD_VIEW) { if (t && t->cmd) t->cmd(t, 1, a - A_WINCMD_VIEW); return; }
    if (a >= A_WINCMD_FILE) { if (t && t->cmd) t->cmd(t, 0, a - A_WINCMD_FILE); return; }
    char p[160]; Cred c;
    switch (a) {
    case A_ABOUT: dialog_show_about(); break;
    case A_SYNC: {
        int r = fs_sync();
        dialog_msg("Save to Disk", r == 0 ? "Filesystem saved to disk." : "Failed, or there is no ATA disk.", "");
        break; }
    case A_REBOOT: fs_sync(); sys_reboot(); break;
    case A_POWEROFF: fs_sync(); sys_poweroff(); break;
    case A_NEW_TERM:  launch_prog("terminal", NULL); break;
    case A_NEW_FILES: launch_prog("files", "/home/user"); break;
    case A_NEW_EDIT:  launch_prog("editor", NULL); break;
    case A_NEW_MON:   launch_prog("sysmon", NULL); break;
    case A_NEW_PAINT: launch_prog("paint", NULL); break;
    case A_DA_ALARM:   launch_single("alarm"); break;
    case A_DA_CALC:    launch_single("calc"); break;
    case A_DA_CONTROL: launch_single("control"); break;
    case A_DA_KEYCAPS: launch_single("keycaps"); break;
    case A_DA_NOTEPAD: launch_single("notepad"); break;
    case A_DA_PUZZLE:  launch_single("puzzle"); break;
    case A_DA_SCRAP:   launch_single("scrapbook"); break;
    case A_SHOW_CLIP:  launch_single("clipview"); break;
    case A_CLOSE: if (t) wm_close(t); break;
    case A_ZOOM: if (t) wm_zoom(t); break;
    case A_NEXT: wm_next(); break;
    case A_HIDE: if (t) wm_hide(t); break;
    case A_HIDE_OTHERS: if (t) wm_hide_others(t); break;
    case A_SHOW_ALL: wm_show_all(); break;
    case A_DK_OPEN: open_icon(desk_sel); break;
    case A_DK_INFO: if (!icon_path(desk_sel, p, sizeof p)) launch_prog("getinfo", p); break;
    case A_DK_DUP:
        if (desktop_sel_kind() == 1 && !icon_path(desk_sel, p, sizeof p)) {
            c = cred_set(GOS_UID_USER); int r = fs_duplicate(p); cred_back(c);
            if (r) report("Duplicate", r); else scan_icons();
        }
        break;
    case A_DK_TRASH:
        if (desktop_sel_kind() == 1 && !icon_path(desk_sel, p, sizeof p)) {
            c = cred_set(GOS_UID_USER); int r = trash_move(p); cred_back(c);
            if (r) report("Move to Trash", r); else scan_icons();
        }
        break;
    case A_DK_NEWFOLDER: {
        char nm[VFS_NAME + 8];
        c = cred_set(GOS_UID_USER);
        int r = fs_unique_name(DESK_DIR, "untitled folder", "", nm, sizeof nm);
        if (!r) { ksnprintf(p, sizeof p, "%s/%s", DESK_DIR, nm); r = vfs_mkdir(NULL, p); }
        cred_back(c);
        if (r) report("New Folder", r);
        else { scan_icons(); for (int i = 0; i < ndi; i++) if (!strcmp(di[i].name, nm)) desk_sel = i; desk_focus = 1; }
        break; }
    case A_CLEANUP:
        nlay = 0; vfs_unlink(NULL, LAYOUT); scan_icons();
        break;
    case A_EMPTY_TRASH: {
        u32 n, b; trash_info(&n, &b);
        if (!n) { dialog_msg("Empty Trash", "The Trash is already empty.", ""); break; }
        char l1[64]; ksnprintf(l1, sizeof l1, "Permanently delete %u item(s) (%u K) in the Trash?", n, (b + 1023) / 1024);
        dialog_ask("Empty Trash", l1, "This can't be undone.", "Empty", A_EMPTY_TRASH_OK);
        break; }
    case A_EMPTY_TRASH_OK: {
        c = cred_set(GOS_UID_USER); int r = trash_empty(); cred_back(c);
        if (r) report("Empty Trash", r);
        scan_icons();
        break; }
    }
}

static void move_into(const char *src, const char *dstdir) {
    const char *base = strrchr(src, '/'); base = base ? base + 1 : src;
    char dst[256]; ksnprintf(dst, sizeof dst, "%s/%s", !strcmp(dstdir, "/") ? "" : dstdir, base);
    if (vfs_lookup(NULL, dst)) { report("Move", -EEXIST); return; }
    int r = vfs_rename(NULL, src, dst);
    if (r) report("Move", r);
}

static void finish_drag(void) {
    char tgt[160];
    drag_on = 0; mode = D_NONE;
    Window *src = drag_src_win ? wm_find(drag_src_win) : NULL;
    Window *w = wm_at(mx, my);
    Cred c = cred_set(drag_uid);
    if (w) {
        if (w->owner && drag_path[0]) {
            int cx, cy, cw, ch; wm_client(w, &cx, &cy, &cw, &ch);
            GEvent ev = { GE_DROP, mx - cx, my - cy, 0, 0 }; uwin_post(w, &ev);
        }
    } else if (my >= GUI_MB) {
        int t = icon_at(mx, my);
        if (t >= 0 && t != drag_src_di && drag_path[0]) {
            if (di[t].kind == DK_TRASH) { int r = trash_move(drag_path); if (r) report("Move to Trash", r); }
            else if (is_folder_icon(t) && !icon_path(t, tgt, sizeof tgt)) move_into(drag_path, tgt);
            else desktop_alert();
        } else if (t < 0 || t == drag_src_di) {
            if (drag_src_di >= 0) lay_set(di[drag_src_di].name, mx - 16, my - 16);
            else if (drag_path[0]) {
                const char *base = strrchr(drag_path, '/'); base = base ? base + 1 : drag_path;
                char nm[VFS_NAME]; strlcpy(nm, base, sizeof nm);
                move_into(drag_path, DESK_DIR);
                if (vfs_lookup(NULL, "/home/user/Desktop")) lay_set(nm, mx - 16, my - 16);
            }
        }
    }
    cred_back(c);
    if (src && src->mouse) {
        int cx, cy, cw, ch; wm_client(src, &cx, &cy, &cw, &ch);
        src->mouse(src, EV_MOUSE_UP, mx - cx, my - cy, 0);
    }
    drag_src_win = 0; drag_src_di = -1;
    scan_icons();
    dirty |= DIRTY_SCENE;
}

int desktop_drag_begin(Task *t, const char *abs) {
    Window *w = mode == D_CLIENT ? wm_find(drag_id) : NULL;
    if (!w || w->owner != t || !btn) return -EPERM;
    VNode *n = vfs_lookup(NULL, abs);
    if (!n) return -ENOENT;
    strlcpy(drag_path, abs, sizeof drag_path);
    drag_icon = icon_for(n); drag_on = 1; drag_src_di = -1; drag_src_win = w->id; drag_uid = t->uid; mode = D_DRAG;
    press_x = mx; press_y = my;
    dirty |= DIRTY_SCENE;
    return 0;
}
int desktop_drag_path(char *out, int max) { strlcpy(out, drag_path, (u32)max); return (int)strlen(out); }

static void drag_mouse(const Event *e) {
    dirty |= DIRTY_SCENE;
    if (mode == D_ICON) {
        if (e->type == EV_MOUSE_UP) { mode = D_NONE; return; }
        if (abs_i(mx - press_x) + abs_i(my - press_y) > 4) {
            drag_on = 1; drag_src_di = icon_press; drag_src_win = 0; drag_uid = GOS_UID_USER; mode = D_DRAG;
            drag_icon = di[icon_press].icon;
            if (di[icon_press].kind == DK_FILE) icon_path(icon_press, drag_path, sizeof drag_path);
            else drag_path[0] = 0;
        }
        return;
    }
    if (e->type == EV_MOUSE_UP) finish_drag();
}

static void client_local(Window *w, int *lx, int *ly) {
    int cx, cy, cw, ch; wm_client(w, &cx, &cy, &cw, &ch);
    *lx = mx - cx; *ly = my - cy;
}

static void click(void) {
    dirty |= DIRTY_SCENE;
    if (dialog_visible()) { int a = dialog_click(mx, my); if (a) do_action(a); return; }
    if (menu_current() >= 0) {
        int it = menu_item_hit(mx, my);
        int a = it >= 0 ? menu_item_action(it) : 0;
        menu_close();
        if (a) do_action(a);
        return;
    }
    if (my < GUI_MB) { int m = menu_bar_hit(mx, my); if (m >= 0) menu_open(m); return; }

    Window *w = wm_at(mx, my);
    if (!w) {
        desk_focus = 1;
        int i = icon_at(mx, my);
        u32 now = timer_ms_now();
        if (i >= 0 && i == last_click_di && now - last_click_ms < (u32)set_dblclick_ms()) {
            last_click_di = -1; desk_sel = i;
            open_icon(i);
            return;
        }
        last_click_di = i; last_click_ms = now; desk_sel = i;
        if (i >= 0) { mode = D_ICON; icon_press = i; press_x = mx; press_y = my; }
        return;
    }
    desk_focus = 0; desk_sel = -1; last_click_di = -1;
    wm_focus(w);
    int h = wm_hit(w, mx, my);
    switch (h) {
    case WH_CLOSE: wm_close(w); break;
    case WH_ZOOM:  wm_zoom(w); break;
    case WH_TITLE: if (!w->zoomed) { mode = D_MOVE; drag_id = w->id; dox = mx - w->x; doy = my - w->y; } break;
    case WH_RESIZE: if (w->resizable && !w->zoomed) { mode = D_RESIZE; drag_id = w->id; ox = mx - w->w; oy = my - w->h; } break;
    case WH_VSCROLL: {
        int sx, sy, sh; wm_vscroll_geom(w, &sx, &sy, &sh);
        int part = widget_vscroll_part(sh, w->sc_pos, w->sc_total, w->sc_view, my - sy, &grab);
        if (part == SB_UP) wm_scroll_by(w, -1);
        else if (part == SB_DOWN) wm_scroll_by(w, 1);
        else if (part == SB_PGUP) wm_scroll_by(w, -w->sc_view);
        else if (part == SB_PGDN) wm_scroll_by(w, w->sc_view);
        else if (part == SB_THUMB) { mode = D_THUMB; drag_id = w->id; }
        break; }
    case WH_CLIENT:
        mode = D_CLIENT; drag_id = w->id; btn = 1;
        if (w->mouse) { int lx, ly; client_local(w, &lx, &ly); w->mouse(w, EV_MOUSE_DOWN, lx, ly, 1); }
        break;
    }
}

static void on_mouse(const Event *e) {
    mx = e->x; my = e->y;
    if (e->type == EV_MOUSE_DOWN) click();
    btn = e->buttons & 1;
    if (mode == D_ICON || mode == D_DRAG) { drag_mouse(e); dirty |= DIRTY_CURSOR; return; }
    Window *w = mode != D_NONE ? wm_find(drag_id) : NULL;
    if (mode != D_NONE && !w) mode = D_NONE;
    if (e->type == EV_MOUSE_UP) {
        if (mode == D_CLIENT && w && w->mouse) { int lx, ly; client_local(w, &lx, &ly); w->mouse(w, EV_MOUSE_UP, lx, ly, 0); }
        mode = D_NONE; dirty |= DIRTY_SCENE;
    } else if (btn && w) {
        if (mode == D_MOVE) wm_move(w, mx - dox, my - doy);
        else if (mode == D_RESIZE) wm_resize(w, mx - ox, my - oy);
        else if (mode == D_THUMB) {
            int sx, sy, sh; wm_vscroll_geom(w, &sx, &sy, &sh);
            if (w->sc_total > w->sc_view) { w->sc_pos = widget_vscroll_pos(sh, w->sc_total, w->sc_view, my - sy, grab); wm_scroll_by(w, 0); }
        } else if (mode == D_CLIENT && w->mouse) { int lx, ly; client_local(w, &lx, &ly); w->mouse(w, EV_MOUSE_MOVE, lx, ly, 1); }
        dirty |= DIRTY_SCENE;
    }
    if (menu_current() >= 0 || dialog_visible()) dirty |= DIRTY_SCENE;
    dirty |= DIRTY_CURSOR;
}

static void on_wheel(const Event *e) {
    mx = e->x; my = e->y;
    if (dialog_visible() || menu_current() >= 0) return;
    Window *w = wm_at(mx, my);
    if (!w || w->sc_total <= w->sc_view) return;
    wm_scroll_by(w, e->a * 3);
    dirty |= DIRTY_SCENE;
}

static void on_key(const Event *e) {
    int k = e->a, m = e->b;
    dirty |= DIRTY_SCENE;
    if (dialog_visible()) { int a = dialog_key(k); if (a) do_action(a); return; }
    if (menu_current() >= 0) { if (k == 27) menu_close(); return; }
    if (m & MOD_ALT) {
        switch (k) {
        case '\t': wm_next(); return;
        case 't': launch_prog("terminal", NULL); return;
        case 'f': launch_prog("files", "/home/user"); return;
        case 'e': launch_prog("editor", NULL); return;
        case 'm': launch_prog("sysmon", NULL); return;
        case 'p': launch_prog("paint", NULL); return;
        case 'c': launch_single("calc"); return;
        case 'w': case KEY_F4: { Window *t = wm_top(); if (t) wm_close(t); return; }
        }
    }
    if (desk_focus && desk_sel >= 0 && !(m & (MOD_CTRL | MOD_ALT))) {
        if (k == '\n') { open_icon(desk_sel); return; }
        if (k == KEY_DEL && desktop_sel_kind() == 1) { do_action(A_DK_TRASH); return; }
    }
    Window *t = wm_top();
    if (t && t->key) t->key(t, k, m);
}

static void on_clock(const Event *e) {
    char n[6] = { '0' + e->a / 10, '0' + e->a % 10, ':', '0' + e->b / 10, '0' + e->b % 10, 0 };
    int ch = 0;
    for (int i = 0; i < 6; i++) if (clk[i] != n[i]) { clk[i] = n[i]; ch = 1; }
    if (ch) dirty |= DIRTY_SCENE | DIRTY_CURSOR;
}

void desktop_handle_event(const Event *e) {
    switch (e->type) {
    case EV_MOUSE_MOVE: case EV_MOUSE_DOWN: case EV_MOUSE_UP: on_mouse(e); break;
    case EV_MOUSE_WHEEL: on_wheel(e); break;
    case EV_KEY_DOWN: on_key(e); break;
    case EV_CLOCK: on_clock(e); break;
    default: break;
    }
}

void desktop_tick(void) {
    cpu_load_sample();
    task_reap();
    if (flash > 0) { flash--; dirty |= DIRTY_SCENE; }
    if (vfs_generation() != di_gen) { scan_icons(); dirty |= DIRTY_SCENE; }
    for (int i = 0; i < wm_count(); i++) {
        Window *w = wm_nth(i);
        if (w && w->tick && w->tick(w)) dirty |= DIRTY_SCENE;
    }
    if (dialog_visible()) dirty |= DIRTY_SCENE;
    dirty |= DIRTY_CURSOR;
}

int desktop_take_dirty(void) { u32 f = irq_save(); int d = dirty; dirty = 0; irq_restore(f); return d; }

int desktop_take_dirty_peek(void) { return dirty; }

void desktop_init(int px, int py) {
    mx = px; my = py;
    video_set_palette(C_BLK, 0, 0, 0);     video_set_palette(C_WHT, 255, 255, 255);
    video_set_palette(C_WIN, 255, 255, 255);
    video_set_palette(C_RED, 230, 20, 20); video_set_palette(C_GRY, 150, 150, 150);
    video_set_palette(C_DGR, 90, 90, 90);   video_set_palette(C_BLU, 30, 60, 170);
    video_set_palette(C_GRN, 40, 170, 70);  video_set_palette(C_YEL, 240, 220, 60);
    video_set_palette(C_CYN, 40, 190, 200); video_set_palette(C_LGR, 210, 210, 210);
    desktop_apply_color(settings_get(SET_DESK_COLOR));
    lay_load();
    desk_focus = 1;
    scan_icons();
    dirty = DIRTY_SCENE | DIRTY_CURSOR;
}
