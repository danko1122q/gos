#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H
#include "lib/list.h"
#include "abi.h"

#define WM_MAXWIN 12
#define WM_MENU_FILE 12
#define WM_MENU_VIEW 8
#define WM_MENU_EDIT 8

typedef struct Window Window;
struct Window {
    ListNode node;
    int id;
    int x, y, w, h;
    int rx, ry, rw, rh, zoomed;
    int minw, minh, resizable;
    int hidden;
    char title[40];
    char info[80]; int has_info;
    int sc_pos, sc_total, sc_view;
    void *priv;
    void (*draw)(Window *w, int x, int y, int cw, int ch);
    void (*mouse)(Window *w, int ev, int lx, int ly, int btn);
    void (*key)(Window *w, int key, int mods);
    int  (*tick)(Window *w);
    int  (*close_req)(Window *w);
    void (*on_close)(Window *w);
    const char **file_items, **view_items, **edit_items;
    void (*cmd)(Window *w, int menu, int idx);

    struct Task *owner;
    GEvent evq[32]; u32 evh, evt;
    u8 *surf; u32 surf_va; int slot;
    int ksl, surf_pages;
    int close_pending, sc_notified, last_cw, last_ch;
    char fitem_s[WM_MENU_FILE][32], vitem_s[WM_MENU_VIEW][32], eitem_s[WM_MENU_EDIT][32];
    const char *fitem_p[WM_MENU_FILE + 1], *vitem_p[WM_MENU_VIEW + 1], *eitem_p[WM_MENU_EDIT + 1];
};

enum { WH_NONE = 0, WH_CLOSE, WH_TITLE, WH_ZOOM, WH_CLIENT, WH_VSCROLL, WH_RESIZE, WH_INFO };

void    wm_init(void);
Window *wm_create(const char *title, int x, int y, int w, int h);
void    wm_close(Window *w);
void    wm_destroy(Window *w);
void    wm_focus(Window *w);
Window *wm_top(void);
Window *wm_at(int x, int y);
Window *wm_find(int id);
int     wm_count(void);
Window *wm_nth(int i);
int     wm_hit(Window *w, int x, int y);
void    wm_client(const Window *w, int *x, int *y, int *cw, int *ch);
void    wm_vscroll_geom(const Window *w, int *sx, int *sy, int *sh);
void    wm_zoom(Window *w);
void    wm_next(void);
void    wm_hide(Window *w);
void    wm_hide_others(Window *keep);
void    wm_show_all(void);
void    wm_move(Window *w, int x, int y);
void    wm_resize(Window *w, int nw, int nh);
void    wm_scroll_by(Window *w, int delta);
void    wm_set_info(Window *w, const char *s);
void    wm_draw_all(void);

Window *uwin_create(struct Task *t, const char *title, int x, int y, int w, int h);
Window *uwin_find(struct Task *t, int id);
void    uwin_destroy_owned(struct Task *t);
void    uwin_close(Window *w);
void    uwin_post(Window *w, const GEvent *e);
int     uwin_wait(Window *w, GEvent *e, int timeout_ms);
void    uwin_info(Window *w, GWinInfo *gi);
int     uwin_ctl(Window *w, int cmd, const char *s, int a, int b, int c);
int     uwin_set_menu(Window *w, const char *files, const char *views);
void    uwin_resized(Window *w);
void    uwin_scrolled(Window *w);

#endif
