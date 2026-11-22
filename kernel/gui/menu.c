#include "gui/menu.h"
#include "gui/metrics.h"
#include "gui/window.h"
#include "gui/desktop.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/icons.h"
#include "drivers/video.h"
#include "proc/task.h"
#include "lib/string.h"

#define MENU_X0 44
static const char *titles[MENU_COUNT] = { "", "File", "Edit", "View", "Special", "" };

enum { F_SEP = 1, F_DIS = 2, F_CHK = 4 };
typedef struct { char label[40]; int act, flags; } Item;

static Item items[32];
static int nitems, cur = -1;
static char app_name[16];
static int app_icon;

static void app_info(void) {
    Window *t = wm_top();
    const char *n = (t && t->owner && !desktop_focused()) ? t->owner->name : "GOS";
    strlcpy(app_name, n, sizeof app_name);
    if (app_name[0] >= 'a' && app_name[0] <= 'z') app_name[0] = (char)(app_name[0] - 32);
    app_icon = !strcmp(n, "terminal") ? ICON_TERM : !strcmp(n, "sysmon") ? ICON_MONITOR : !strcmp(n, "files") ? ICON_FOLDER
             : !strcmp(n, "GOS") ? ICON_DRIVE : ICON_APP;
    if (app_name[0] == 'S' && !strcmp(n, "sysmon")) strlcpy(app_name, "Monitor", sizeof app_name);
}

static void title_span(int i, int *x0, int *x1) {
    if (i == MENU_APPLE) { *x0 = 0; *x1 = 44; return; }
    if (i == MENU_APP) {
        int w = 16 + ftext_width(FACE_CHICAGO, app_name) + 8;
        *x1 = SCREEN_W - 64; *x0 = *x1 - w; return;
    }
    int x = MENU_X0;
    for (int k = MENU_FILE; k < i; k++) x += ftext_width(FACE_CHICAGO, titles[k]) + 16;
    *x0 = x; *x1 = x + ftext_width(FACE_CHICAGO, titles[i]) + 16;
}
static int dd_left(void) {
    int x0, x1; title_span(cur, &x0, &x1);
    return x0;
}
static int item_h(int i) { return (items[i].flags & F_SEP) ? 8 : 20; }
static int dd_w(void) { int mw = 0; for (int i = 0; i < nitems; i++) { int l = ftext_width(FACE_CHICAGO, items[i].label); if (l > mw) mw = l; } return mw + 36; }
static int dd_h(void) { int h = 4; for (int i = 0; i < nitems; i++) h += item_h(i); return h; }
static int dd_x(void) { int x = dd_left(), w = dd_w(); if (x + w > SCREEN_W - 2) x = SCREEN_W - 2 - w; return x; }
static int item_y(int i) { int y = GUI_MB + 2; for (int k = 0; k < i; k++) y += item_h(k); return y; }

static void add(const char *label, int act, int flags) {
    if (nitems >= 32) return;
    strlcpy(items[nitems].label, label, sizeof items[0].label);
    items[nitems].act = act; items[nitems].flags = flags; nitems++;
}
static void sep(void) { add("", 0, F_SEP); }

static void build(int m) {
    nitems = 0;
    Window *top = wm_top();
    int desk = desktop_focused();
    switch (m) {
    case MENU_APPLE:
        add("About GOS...", A_ABOUT, 0); sep();
        add("Alarm Clock", A_DA_ALARM, 0); add("Calculator", A_DA_CALC, 0); add("Control Panel", A_DA_CONTROL, 0);
        add("Key Caps", A_DA_KEYCAPS, 0); add("Note Pad", A_DA_NOTEPAD, 0); add("Puzzle", A_DA_PUZZLE, 0);
        add("Scrapbook", A_DA_SCRAP, 0); sep();
        add("Terminal", A_NEW_TERM, 0); add("File Manager", A_NEW_FILES, 0); add("Text Editor", A_NEW_EDIT, 0);
        add("Paint", A_NEW_PAINT, 0); add("System Monitor", A_NEW_MON, 0);
        break;
    case MENU_FILE:
        if (desk) {
            int k = desktop_sel_kind();
            add("New Folder", A_DK_NEWFOLDER, 0);
            add("Open", A_DK_OPEN, k ? 0 : F_DIS);
            add("Get Info", A_DK_INFO, k ? 0 : F_DIS);
            add("Duplicate", A_DK_DUP, k == 1 ? 0 : F_DIS);
            add("Move to Trash", A_DK_TRASH, k == 1 ? 0 : F_DIS);
        } else if (top && top->file_items) {
            for (int i = 0; top->file_items[i]; i++) add(top->file_items[i], A_WINCMD_FILE + i, 0);
        }
        if (nitems) sep();
        add("Close Window", A_CLOSE, top ? 0 : F_DIS);
        break;
    case MENU_EDIT:
        if (!desk && top && top->edit_items) {
            for (int i = 0; top->edit_items[i]; i++) add(top->edit_items[i], A_WINCMD_EDIT + i, 0);
        } else {
            add("Undo", 0, F_DIS); add("Cut", 0, F_DIS); add("Copy", 0, F_DIS); add("Paste", 0, F_DIS); add("Clear", 0, F_DIS);
        }
        sep(); add("Show Clipboard", A_SHOW_CLIP, 0);
        break;
    case MENU_VIEW:
        if (!desk && top && top->view_items) {
            for (int i = 0; top->view_items[i]; i++) add(top->view_items[i], A_WINCMD_VIEW + i, 0);
            sep();
        }
        add("Zoom Window", A_ZOOM, (!desk && top && top->resizable) ? 0 : F_DIS);
        add("Next Window", A_NEXT, wm_count() > 1 ? 0 : F_DIS);
        break;
    case MENU_SPECIAL:
        add("Clean Up Desktop", A_CLEANUP, 0);
        add("Empty Trash...", A_EMPTY_TRASH, 0); sep();
        add("Save to Disk", A_SYNC, 0); add("Restart", A_REBOOT, 0); add("Shut Down", A_POWEROFF, 0);
        break;
    case MENU_APP:
        if (!wm_count()) add("(no windows)", 0, F_DIS);
        for (int i = wm_count() - 1; i >= 0; i--) {
            Window *w = wm_nth(i);
            char l[40]; strlcpy(l, w->title, sizeof l);
            if (w->hidden && strlen(l) < 28) strlcpy(l + strlen(l), " (hidden)", sizeof l - strlen(l));
            add(l, A_FOCUS + w->id, (w == top && !desk) ? F_CHK : 0);
        }
        sep();
        add("Hide Window", A_HIDE, top ? 0 : F_DIS);
        add("Hide Others", A_HIDE_OTHERS, top ? 0 : F_DIS);
        add("Show All", A_SHOW_ALL, wm_count() ? 0 : F_DIS);
        break;
    }
}

int  menu_current(void) { return cur; }
void menu_open(int m) { app_info(); cur = m; build(m); }
void menu_close(void) { cur = -1; }

int menu_bar_hit(int x, int y) {
    if (y >= GUI_MB) return -1;
    app_info();
    for (int i = 0; i < MENU_COUNT; i++) {
        int x0, x1; title_span(i, &x0, &x1);
        if (x >= x0 && x < x1) return i;
    }
    return -1;
}

int menu_item_hit(int x, int y) {
    if (cur < 0) return -1;
    int tx = dd_x(), w = dd_w();
    if (x < tx || x >= tx + w) return -1;
    for (int i = 0; i < nitems; i++) {
        int iy = item_y(i);
        if (y >= iy && y < iy + item_h(i)) return (items[i].flags & (F_SEP | F_DIS)) ? -1 : i;
    }
    return -1;
}
int menu_item_action(int idx) { return (idx >= 0 && idx < nitems) ? items[idx].act : 0; }

void menu_draw(int mx, int my, const char *clock) {
    int i;
    int inv = desktop_flash_active();
    u8 bg = inv ? C_BLK : C_WHT, fg = inv ? C_WHT : C_BLK;
    app_info();
    draw_noclip();
    draw_rect(0, 0, SCREEN_W, GUI_MB, bg); draw_rect(0, GUI_MB - 1, SCREEN_W, 1, C_BLK);
    if (cur == MENU_APPLE) draw_rect(0, 0, 44, GUI_MB - 1, C_BLK);
    u8 lg = cur == MENU_APPLE ? C_WHT : fg;
    draw_rect(10, 3, 3, 14, lg); draw_rect(16, 3, 3, 14, lg); draw_rect(22, 3, 3, 14, lg); draw_rect(10, 3, 15, 3, lg);
    for (i = MENU_FILE; i < MENU_APP; i++) {
        int x0, x1; title_span(i, &x0, &x1);
        if (cur == i) { draw_rect(x0, 0, x1 - x0, GUI_MB - 1, C_BLK); ftext_draw(FACE_CHICAGO, x0 + 8, GUI_MB_TY, titles[i], C_WHT, -1); }
        else ftext_draw(FACE_CHICAGO, x0 + 8, GUI_MB_TY, titles[i], fg, -1);
    }
    {
        int x0, x1; title_span(MENU_APP, &x0, &x1);
        if (cur == MENU_APP) draw_rect(x0, 0, x1 - x0, GUI_MB - 1, C_BLK);
        icon_draw_small(x0 + 4, 2, app_icon);
        ftext_draw(FACE_CHICAGO, x0 + 20, GUI_MB_TY, app_name, cur == MENU_APP ? C_WHT : fg, -1);
    }
    ftext_draw(FACE_CHICAGO, SCREEN_W - 8 - ftext_width(FACE_CHICAGO, clock), GUI_MB_TY, clock, fg, -1);
    if (cur < 0) return;
    int tx = dd_x(), w = dd_w(), h = dd_h();
    draw_rect(tx + 1, GUI_MB + 1, w, h, C_BLK);
    draw_box(tx, GUI_MB, w, h, C_WHT, C_BLK);
    for (i = 0; i < nitems; i++) {
        int iy = item_y(i);
        if (items[i].flags & F_SEP) { for (int xx = tx + 2; xx < tx + w - 2; xx += 2) draw_px(xx, iy + 3, C_BLK); continue; }
        int dis = items[i].flags & F_DIS;
        int hot = !dis && mx >= tx && mx < tx + w && my >= iy && my < iy + 20;
        if (hot) draw_rect(tx + 1, iy, w - 2, 20, C_BLK);
        u8 col = hot ? C_WHT : (dis ? C_GRY : C_BLK);
        if (items[i].flags & F_CHK) ftext_draw(FACE_CHICAGO, tx + 6, iy + 2, "\xFB", col, -1);
        ftext_draw(FACE_CHICAGO, tx + 20, iy + 2, items[i].label, col, -1);
    }
}
