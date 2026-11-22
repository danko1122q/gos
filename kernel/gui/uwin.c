#include "gui/window.h"
#include "gui/desktop.h"
#include "gui/metrics.h"
#include "gui/event.h"
#include "gfx/draw.h"
#include "drivers/video.h"
#include "drivers/keyboard.h"
#include "proc/task.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "arch/io.h"
#include "arch/timer.h"
#include "lib/klog.h"
#include "lib/string.h"
#include "lib/errno.h"

#define QMASK 31

void uwin_post(Window *w, const GEvent *e) {
    if (!w->owner) return;
    u32 f = irq_save();
    if (e->type == GE_MOUSE_MOVE && w->evh != w->evt) {
        GEvent *last = &w->evq[(w->evh - 1) & QMASK];
        if (last->type == GE_MOUSE_MOVE) { *last = *e; irq_restore(f); task_wake(w->owner); return; }
    }
    if (w->evh - w->evt > QMASK) w->evt++;
    w->evq[w->evh++ & QMASK] = *e;
    irq_restore(f);
    task_wake(w->owner);
}

static int pop(Window *w, GEvent *e) {
    u32 f = irq_save();
    if (w->evh == w->evt) { irq_restore(f); return 0; }
    *e = w->evq[w->evt++ & QMASK];
    if (e->type == GE_CLOSE) w->close_pending = 0;
    irq_restore(f);
    return 1;
}

int uwin_wait(Window *w, GEvent *e, int timeout_ms) {
    Task *t = task_current();
    u32 deadline = timer_ticks() + (timeout_ms > 0 ? (u32)(timeout_ms + 9) / 10 : 0);
    for (;;) {
        if (pop(w, e)) return 1;
        if (timeout_ms == 0) return 0;
        u32 f = irq_save();
        if (w->evh != w->evt) { irq_restore(f); continue; }
        if (timeout_ms > 0 && (s32)(timer_ticks() - deadline) >= 0) { irq_restore(f); return 0; }
        t->ev_wait = 1; t->state = T_SLEEP;
        t->wake = timeout_ms > 0 ? deadline : timer_ticks() + 0x7FFFFFFFu;
        irq_restore(f);
        task_yield();
        t->ev_wait = 0;
    }
}

static u32 ks_used;

static int surf_rows(const Window *w) { int r = (int)((u32)w->surf_pages * PAGE_SIZE / GOS_SURF_W); return r > GOS_SURF_H ? GOS_SURF_H : r; }
static int rows_to_pages(int rows) { if (rows > GOS_SURF_H) rows = GOS_SURF_H; if (rows < 1) rows = 1; return (rows * GOS_SURF_W + PAGE_SIZE - 1) / PAGE_SIZE; }

static void surf_unmap_range(Window *w, int from, int to) {
    Task *t = w->owner;
    for (int i = from; i < to; i++) {
        if (t && t->pd) vmm_unmap(t->pd, w->surf_va + i * PAGE_SIZE, 1, 0);
        u32 pa = vmm_kunmap((u32)w->surf + i * PAGE_SIZE);
        if (pa) pmm_free(pa);
    }
}

static int surf_map(Window *w, int from, int to) {
    Task *t = w->owner;
    for (int i = from; i < to; i++) {
        u32 kva = (u32)w->surf + i * PAGE_SIZE, f = pmm_alloc();
        if (!f) { surf_unmap_range(w, from, i); return -1; }
        if (vmm_kmap(kva, f) < 0) { pmm_free(f); surf_unmap_range(w, from, i); return -1; }
        if (vmm_map(t->pd, w->surf_va + i * PAGE_SIZE, f, PTE_U | PTE_W | PTE_SHARED) < 0) {
            pmm_free(vmm_kunmap(kva)); surf_unmap_range(w, from, i); return -1;
        }
        memset((void *)kva, C_WIN, PAGE_SIZE);
    }
    return 0;
}

static void surf_ensure(Window *w, int rows) {
    int need = rows_to_pages(rows);
    if (need <= w->surf_pages) return;
#ifdef GOS_MEMBENCH
    klog("uwin: surface %d -> %d pages (client %d rows)\n", w->surf_pages, need, rows);
#endif
    if (surf_map(w, w->surf_pages, need) == 0) w->surf_pages = need;
}

static void u_draw(Window *w, int x, int y, int cw, int ch) {
    u8 *dst = video_scene();
    int sw = cw < GOS_SURF_W ? cw : GOS_SURF_W, sh = ch < GOS_SURF_H ? ch : GOS_SURF_H;
    if (sh > surf_rows(w)) sh = surf_rows(w);
    int x0 = x < 0 ? -x : 0, x1 = sw;
    if (x + x1 > SCREEN_W) x1 = SCREEN_W - x;
    if (x1 <= x0) return;
    for (int j = 0; j < sh; j++) {
        int py = y + j;
        if (py < 0 || py >= SCREEN_H) continue;
        memcpy(dst + py * SCREEN_W + x + x0, w->surf + j * GOS_SURF_W + x0, x1 - x0);
    }
}
static void u_mouse(Window *w, int ev, int lx, int ly, int btn) {
    GEvent e = { ev == EV_MOUSE_DOWN ? GE_MOUSE_DOWN : ev == EV_MOUSE_UP ? GE_MOUSE_UP : GE_MOUSE_MOVE, lx, ly, btn, kbd_mods() };
    uwin_post(w, &e);
}
static void u_key(Window *w, int key, int mods) { GEvent e = { GE_KEY, key, mods, 0, 0 }; uwin_post(w, &e); }
static void u_cmd(Window *w, int menu, int idx) { GEvent e = { GE_CMD, menu, idx, 0, 0 }; uwin_post(w, &e); }

static int u_close_req(Window *w) {
    if (w->close_pending) { Task *t = w->owner; task_kill(t->pid); return 0; }
    w->close_pending = 1;
    GEvent e = { GE_CLOSE, 0, 0, 0, 0 };
    uwin_post(w, &e);
    return 0;
}

static void u_on_close(Window *w) {
    Task *t = w->owner;
    surf_unmap_range(w, 0, w->surf_pages);
    w->surf_pages = 0;
    ks_used &= ~(1u << w->ksl);
    if (t && t->nwin > 0) t->nwin--;
}

Window *uwin_create(Task *t, const char *title, int x, int y, int w, int h) {
    if (t->nwin >= USER_SURF_SLOTS) return NULL;
    int used = 0;
    for (int i = 0; i < wm_count(); i++) { Window *o = wm_nth(i); if (o->owner == t) used |= 1 << o->slot; }
    int slot = 0;
    while (slot < USER_SURF_SLOTS && (used & (1 << slot))) slot++;
    if (slot >= USER_SURF_SLOTS) return NULL;
    Window *win = wm_create(title, x, y, w, h);
    if (!win) return NULL;
    int ksl = 0;
    while (ksl < (int)KSURF_SLOTS && (ks_used & (1u << ksl))) ksl++;
    if (ksl >= (int)KSURF_SLOTS) { wm_destroy(win); return NULL; }
    win->owner = t; win->ksl = ksl; win->surf_pages = 0;
    win->surf = (u8 *)(vmm_ksurf_base() + (u32)ksl * KSURF_STRIDE);
    win->surf_va = USER_SURF_BASE + slot * USER_SURF_STRIDE;
    int cx0, cy0, cw0, ch0; wm_client(win, &cx0, &cy0, &cw0, &ch0);
    int pages = rows_to_pages(ch0);
    if (surf_map(win, 0, pages)) { win->owner = NULL; wm_destroy(win); return NULL; }
    win->surf_pages = pages;
    ks_used |= 1u << ksl;
    win->slot = slot;
    win->draw = u_draw; win->mouse = u_mouse; win->key = u_key; win->cmd = u_cmd;
    win->close_req = u_close_req; win->on_close = u_on_close;
    win->tick = NULL;
    t->nwin++;
    int cx, cy, cw, ch; wm_client(win, &cx, &cy, &cw, &ch);
    win->last_cw = cw; win->last_ch = ch;
    wm_focus(win);
    desktop_invalidate();
    return win;
}

Window *uwin_find(Task *t, int id) {
    Window *w = wm_find(id);
    return (w && w->owner == t) ? w : NULL;
}

void uwin_destroy_owned(Task *t) {
    preempt_disable();
    for (;;) {
        Window *victim = NULL;
        for (int i = 0; i < wm_count(); i++) { Window *w = wm_nth(i); if (w->owner == t) { victim = w; break; } }
        if (!victim) break;
        wm_destroy(victim);
    }
    desktop_invalidate();
    preempt_enable();
}

void uwin_close(Window *w) {
    preempt_disable();
    wm_destroy(w);
    desktop_invalidate();
    preempt_enable();
}

void uwin_info(Window *w, GWinInfo *gi) {
    int cx, cy, cw, ch; wm_client(w, &cx, &cy, &cw, &ch);
    if (ch > surf_rows(w)) ch = surf_rows(w);
    gi->id = w->id; gi->cw = cw; gi->ch = ch; gi->pitch = GOS_SURF_W;
    gi->x = w->x; gi->y = w->y; gi->w = w->w; gi->h = w->h; gi->zoomed = w->zoomed; gi->surf = w->surf_va;
}

void uwin_resized(Window *w) {
    int cx, cy, cw, ch; wm_client(w, &cx, &cy, &cw, &ch);
    if (cw == w->last_cw && ch == w->last_ch) return;
    w->last_cw = cw; w->last_ch = ch;
    surf_ensure(w, ch);
    if (ch > surf_rows(w)) ch = surf_rows(w);
    GEvent e = { GE_RESIZE, cw, ch, 0, 0 };
    uwin_post(w, &e);
}

void uwin_scrolled(Window *w) {
    if (w->sc_pos == w->sc_notified) return;
    w->sc_notified = w->sc_pos;
    GEvent e = { GE_SCROLL, w->sc_pos, 0, 0, 0 };
    uwin_post(w, &e);
}

static void parse_items(const char *s, char store[][32], const char **ptr, int n) {
    int k = 0;
    while (s && *s && k < n) {
        int l = 0;
        while (*s && *s != '\n') { if (l < 31) store[k][l++] = *s; s++; }
        store[k][l] = 0; ptr[k] = store[k]; k++;
        if (*s == '\n') s++;
    }
    ptr[k] = NULL;
}

int uwin_ctl(Window *w, int cmd, const char *s, int a, int b, int c) {
    switch (cmd) {
    case WC_TITLE:   if (!s) return -EINVAL; strlcpy(w->title, s, sizeof w->title); break;
    case WC_INFO:    if (!s) return -EINVAL; strlcpy(w->info, s, sizeof w->info); break;
    case WC_EDITMENU:
        if (!s) return -EINVAL;
        parse_items(s, w->eitem_s, w->eitem_p, WM_MENU_EDIT);
        w->edit_items = w->eitem_p[0] ? w->eitem_p : NULL;
        break;
    case WC_SCROLL:
        w->sc_total = b < 0 ? 0 : b; w->sc_view = c < 1 ? 1 : c;
        w->sc_pos = a < 0 ? 0 : a;
        { int mx = w->sc_total - w->sc_view; if (mx < 0) mx = 0; if (w->sc_pos > mx) w->sc_pos = mx; }
        w->sc_notified = w->sc_pos;
        break;
    case WC_MINSIZE: w->minw = a < 100 ? 100 : a; w->minh = b < 60 ? 60 : b; break;
    default: return -EINVAL;
    }
    desktop_invalidate();
    return 0;
}

int uwin_set_menu(Window *w, const char *files, const char *views) {
    parse_items(files, w->fitem_s, w->fitem_p, WM_MENU_FILE);
    parse_items(views, w->vitem_s, w->vitem_p, WM_MENU_VIEW);
    w->file_items = w->fitem_p[0] ? w->fitem_p : NULL;
    w->view_items = w->vitem_p[0] ? w->vitem_p : NULL;
    return 0;
}
