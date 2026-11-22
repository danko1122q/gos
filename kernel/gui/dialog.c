#include "gui/dialog.h"
#include "abi.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gui/widget.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "proc/task.h"
#include "arch/timer.h"

enum { K_MSG, K_ASK };
#define MAXLN 10
#define DW 380

static int shown, kind, act_ok, nln, last_result, mono_ln;
static u32 seq_cur, seq_done;
static char ttl[40], ln[MAXLN][64], b_ok[16];

static void settle(int r) { last_result = r; seq_done = seq_cur; }

static void reset(const char *t, int k) {
    if (shown && kind == K_ASK) settle(0);
    strlcpy(ttl, t, sizeof ttl); nln = 0; shown = 1; kind = k; act_ok = 0; mono_ln = 0;
    strlcpy(b_ok, "OK", sizeof b_ok);
    seq_cur++;
}
static void line(const char *s) { if (nln < MAXLN) strlcpy(ln[nln++], s, sizeof ln[0]); }

void dialog_show_about(void) {
    char b[64];
    reset("About GOS", K_MSG); mono_ln = 1;
    line("GOS - Green Operating System " GOS_VERSION);
    ksnprintf(b, sizeof b, "Total Memory: %u MB    Kernel heap: %u/%u KB", mem_total_kb() / 1024, heap_used() / 1024, heap_total() / 1024);
    line(b);
    u32 up = uptime_sec();
    ksnprintf(b, sizeof b, "Free frames: %u KB    Up %u:%02u:%02u", pmm_free_frames() * 4, up / 3600, up / 60 % 60, up % 60);
    line(b);
    line("--- Running applications ---");
    int shown_apps = 0, total = 0;
    for (int i = 0; i < GOS_MAXTASK; i++) {
        Task *t = task_get(i);
        if (!t || !t->pd || t->state == T_ZOMBIE) continue;
        total++;
        if (shown_apps >= 5) continue;
        ksnprintf(b, sizeof b, "%-16s %5u K   %d windows", t->name, vmm_count_user_pages(t->pd) * 4, t->nwin);
        line(b); shown_apps++;
    }
    if (total > shown_apps) { ksnprintf(b, sizeof b, "... and %d other applications", total - shown_apps); line(b); }
}

void dialog_msg(const char *title, const char *l1, const char *l2) {
    reset(title, K_MSG); line(l1); if (l2 && *l2) line(l2);
}

u32 dialog_ask(const char *title, const char *l1, const char *l2, const char *ok, int act) {
    reset(title, K_ASK); line(l1); if (l2 && *l2) line(l2);
    if (ok && *ok) strlcpy(b_ok, ok, sizeof b_ok);
    act_ok = act;
    return seq_cur;
}

int dialog_ask_result(u32 token) {
    if ((s32)(seq_done - token) < 0) return -1;
    return seq_done == token ? last_result : 0;
}

void dialog_close(void) { if (shown && kind == K_ASK) settle(0); shown = 0; }
int  dialog_visible(void) { return shown; }

static int dh(void) { return 84 + nln * 20; }
static int dx(void) { return (640 - DW) / 2; }
static int dy(void) { return (480 - dh()) / 2; }
static int ok_x(void) { return dx() + DW - 104; }
static int cancel_x(void) { return dx() + DW - 204; }
static int btn_y(void) { return dy() + dh() - 38; }
static int in_btn(int x, int y, int bx) { return x >= bx && x < bx + 84 && y >= btn_y() && y < btn_y() + 26; }

static int finish(int r) {
    int a = (r && kind == K_ASK) ? act_ok : 0;
    settle(r); shown = 0;
    return a;
}

int dialog_click(int x, int y) {
    if (!shown) return 0;
    if (kind == K_MSG) { shown = 0; return 0; }
    if (in_btn(x, y, ok_x())) return finish(1);
    if (in_btn(x, y, cancel_x())) { finish(0); return 0; }
    return 0;
}

int dialog_key(int k) {
    if (!shown) return 0;
    if (kind == K_MSG) { if (k == '\n' || k == 27 || k == ' ') shown = 0; return 0; }
    if (k == '\n') return finish(1);
    if (k == 27) { finish(0); return 0; }
    return 0;
}

static void caution(int x, int y) {
    for (int r = 0; r < 28; r++) {
        int hw = 1 + r * 15 / 27;
        draw_rect(x + 16 - hw, y + 2 + r, 2 * hw + 1, 1, C_WHT);
        draw_px(x + 16 - hw, y + 2 + r, C_BLK); draw_px(x + 16 + hw, y + 2 + r, C_BLK);
    }
    draw_rect(x + 1, y + 29, 31, 1, C_BLK);
    draw_rect(x + 15, y + 11, 3, 10, C_BLK); draw_rect(x + 15, y + 23, 3, 3, C_BLK);
}

void dialog_draw(void) {
    if (!shown) return;
    int w = DW, h = dh(), x = dx(), y = dy();
    draw_noclip();
    draw_rect(x + 8, y + 8, w, h, C_BLK);
    draw_box(x, y, w, h, C_WHT, C_BLK); draw_frame(x + 4, y + 4, w - 8, h - 8, C_BLK);
    ftext_draw_center(FACE_CHICAGO, x + w / 2, y + 12, ttl, C_BLK, -1);
    draw_rect(x + 4, y + 30, w - 8, 1, C_BLK);
    if (kind == K_ASK) {
        caution(x + 16, y + 40);
        for (int i = 0; i < nln; i++) ftext_draw(mono_ln ? FACE_MONO : FACE_CHICAGO, x + 62, y + 40 + i * 20, ln[i], C_BLK, -1);
        widget_button(cancel_x(), btn_y(), 84, 26, "Cancel", 0);
        widget_button_default(ok_x(), btn_y(), 84, 26, b_ok, 0);
    } else {
        for (int i = 0; i < nln; i++) ftext_draw_center(mono_ln ? FACE_MONO : FACE_CHICAGO, x + w / 2, y + 38 + i * 20, ln[i], C_BLK, -1);
        widget_button_default(x + w / 2 - 40, y + h - 38, 80, 24, "OK", 0);
    }
}
