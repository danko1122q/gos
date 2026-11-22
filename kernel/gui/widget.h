#ifndef GUI_WIDGET_H
#define GUI_WIDGET_H

enum { SB_NONE = 0, SB_UP, SB_DOWN, SB_PGUP, SB_PGDN, SB_THUMB };

void widget_titlebar(int x, int y, int w, const char *title, int active, int zoomed);
void widget_infobar(int x, int y, int w, const char *info);
void widget_vscroll(int x, int y, int h, int pos, int total, int view);
int  widget_vscroll_part(int h, int pos, int total, int view, int ly, int *grab);
int  widget_vscroll_pos(int h, int total, int view, int ly, int grab);
void widget_sizebox(int x, int y);
void widget_button(int x, int y, int w, int h, const char *label, int pressed);
void widget_button_default(int x, int y, int w, int h, const char *label, int pressed);

#endif
