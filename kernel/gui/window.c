#include "gui/window.h"
#include "gui/metrics.h"
#include "gui/widget.h"
#include "gfx/draw.h"
#include "drivers/video.h"
#include "mem/heap.h"
#include "lib/string.h"

static ListNode zlist;
static int next_id = 1, count, cascade;

void wm_init(void) { list_init(&zlist); count = 0; }

#define W_OF(n) container_of(n, Window, node)

static int info_h(const Window *w) { return w->has_info ? GUI_IH : 0; }

void wm_client(const Window *w, int *x, int *y, int *cw, int *ch) {
    *x = w->x + 1; *y = w->y + GUI_TH + info_h(w);
    *cw = w->w - 2 - GUI_SB; *ch = w->h - GUI_TH - info_h(w) - 1;
}
void wm_vscroll_geom(const Window *w, int *sx, int *sy, int *sh) {
    *sx = w->x + w->w - GUI_SB - 1; *sy = w->y + GUI_TH + info_h(w);
    *sh = w->h - GUI_TH - info_h(w) - 1 - (w->resizable ? 14 : 0);
}

Window *wm_create(const char *title, int x, int y, int w, int h) {
    if (count >= WM_MAXWIN) return NULL;
    Window *win = kzalloc(sizeof(Window));
    if (!win) return NULL;
    if (w > SCREEN_W) w = SCREEN_W;
    if (h > SCREEN_H - GUI_MB) h = SCREEN_H - GUI_MB;
    if (x < 0) {
        x = 100 + cascade * 24; y = GUI_MB + GUI_WIN0 + cascade * 22;
        cascade = (cascade + 1) % 6;
        if (x + w > SCREEN_W) x = SCREEN_W - w;
        if (y + h > SCREEN_H) y = SCREEN_H - h;
    }
    win->id = next_id++; win->x = x; win->y = y; win->w = w; win->h = h;
    win->minw = 180; win->minh = 110; win->resizable = 1; win->has_info = 1;
    strlcpy(win->title, title, sizeof win->title);
    win->sc_view = 1;
    list_add_tail(&zlist, &win->node);
    count++;
    return win;
}

void wm_destroy(Window *w) {
    if (w->on_close) w->on_close(w);
    list_del(&w->node);
    count--;
    kfree(w);
}
void wm_close(Window *w) {
    if (w->close_req && !w->close_req(w)) return;
    wm_destroy(w);
}

void wm_focus(Window *w) {
    w->hidden = 0;
    if (zlist.prev == &w->node) return;
    list_del(&w->node);
    list_add_tail(&zlist, &w->node);
}
Window *wm_top(void) {
    for (ListNode *n = zlist.prev; n != &zlist; n = n->prev) if (!W_OF(n)->hidden) return W_OF(n);
    return NULL;
}

Window *wm_at(int x, int y) {
    for (ListNode *n = zlist.prev; n != &zlist; n = n->prev) {
        Window *w = W_OF(n);
        if (w->hidden) continue;
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h) return w;
    }
    return NULL;
}
Window *wm_find(int id) {
    ListNode *n; list_for_each(n, &zlist) if (W_OF(n)->id == id) return W_OF(n);
    return NULL;
}
int wm_count(void) { return count; }
Window *wm_nth(int i) {
    ListNode *n; list_for_each(n, &zlist) if (i-- == 0) return W_OF(n);
    return NULL;
}

int wm_hit(Window *w, int x, int y) {
    int lx = x - w->x, ly = y - w->y;
    if (ly < GUI_TH) {
        if (lx >= 5 && lx < 18 && ly >= 4 && ly < 17) return WH_CLOSE;
        if (lx >= w->w - 19 && lx < w->w - 6 && ly >= 4 && ly < 17) return WH_ZOOM;
        return WH_TITLE;
    }
    if (w->has_info && ly < GUI_TH + GUI_IH) return WH_INFO;
    if (lx >= w->w - GUI_SB - 1) {
        if (w->resizable && ly >= w->h - 15) return WH_RESIZE;
        return WH_VSCROLL;
    }
    return WH_CLIENT;
}

static void clamp_pos(Window *w) {
    if (w->x < 40 - w->w) w->x = 40 - w->w;
    if (w->x > SCREEN_W - 40) w->x = SCREEN_W - 40;
    if (w->y < GUI_MB) w->y = GUI_MB;
    if (w->y > SCREEN_H - GUI_TH) w->y = SCREEN_H - GUI_TH;
}
void wm_move(Window *w, int x, int y) { if (w->zoomed) return; w->x = x; w->y = y; clamp_pos(w); }

void wm_resize(Window *w, int nw, int nh) {
    if (nw < w->minw) nw = w->minw;
    if (nh < w->minh) nh = w->minh;
    if (w->x + nw > SCREEN_W) nw = SCREEN_W - w->x;
    if (w->y + nh > SCREEN_H) nh = SCREEN_H - w->y;
    w->w = nw; w->h = nh;
    if (w->owner) uwin_resized(w);
}

void wm_zoom(Window *w) {
    if (!w->resizable) return;
    if (w->zoomed) { w->x = w->rx; w->y = w->ry; w->w = w->rw; w->h = w->rh; w->zoomed = 0; }
    else {
        w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h;
        w->x = 0; w->y = GUI_MB; w->w = SCREEN_W; w->h = SCREEN_H - GUI_MB; w->zoomed = 1;
    }
    if (w->owner) uwin_resized(w);
}

static void to_back(Window *t) {
    list_del(&t->node);
    zlist.next->prev = &t->node; t->node.next = zlist.next; t->node.prev = &zlist; zlist.next = &t->node;
}
void wm_hide(Window *w) { w->hidden = 1; to_back(w); }
void wm_hide_others(Window *keep) {
    ListNode *n;
    list_for_each(n, &zlist) if (W_OF(n) != keep) W_OF(n)->hidden = 1;
}
void wm_show_all(void) { ListNode *n; list_for_each(n, &zlist) W_OF(n)->hidden = 0; }

void wm_next(void) {
    if (count < 2) return;
    Window *t = wm_top();
    if (!t) return;
    list_del(&t->node);
    zlist.next->prev = &t->node; t->node.next = zlist.next; t->node.prev = &zlist; zlist.next = &t->node;
}

void wm_scroll_by(Window *w, int d) {
    int mx = w->sc_total - w->sc_view;
    if (mx < 0) mx = 0;
    w->sc_pos += d;
    if (w->sc_pos > mx) w->sc_pos = mx;
    if (w->sc_pos < 0) w->sc_pos = 0;
    if (w->owner) uwin_scrolled(w);
}

void wm_set_info(Window *w, const char *s) { strlcpy(w->info, s, sizeof w->info); }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void wm_draw_all(void) {
    ListNode *n;
    int top = wm_top() ? wm_top()->id : -1;
    list_for_each(n, &zlist) {
        Window *w = W_OF(n);
        if (w->hidden) continue;
        draw_noclip();
        draw_rect(w->x + 1, w->y + 1, w->w, w->h, C_BLK);
        draw_box(w->x, w->y, w->w, w->h, C_WIN, C_BLK);
        int cx, cy, cw, ch; wm_client(w, &cx, &cy, &cw, &ch);
        int x0 = clampi(cx, 0, SCREEN_W), y0 = clampi(cy, 0, SCREEN_H);
        int x1 = clampi(cx + cw, 0, SCREEN_W), y1 = clampi(cy + ch, 0, SCREEN_H);
        draw_clip(x0, y0, x1, y1);
        if (w->draw) w->draw(w, cx, cy, cw, ch);
        draw_noclip();
        widget_titlebar(w->x, w->y, w->w, w->title, w->id == top, w->zoomed);
        if (w->has_info) widget_infobar(w->x, w->y + GUI_TH, w->w, w->info);
        int sx, sy, sh; wm_vscroll_geom(w, &sx, &sy, &sh);
        widget_vscroll(sx, sy, sh, w->sc_pos, w->sc_total, w->sc_view);
        if (w->resizable) widget_sizebox(sx, sy + sh);
    }
}
