#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H
#include "gui/event.h"
#include "abi.h"

enum { DIRTY_SCENE = 1, DIRTY_CURSOR = 2 };
#define DESK_COLORS GOS_DESK_COLORS

struct Task;

void desktop_init(int pointer_x, int pointer_y);
void desktop_handle_event(const Event *e);
void desktop_tick(void);
int  desktop_take_dirty(void);
int  desktop_take_dirty_peek(void);
void desktop_invalidate(void);
int  desktop_color(void);
void desktop_render(void);

void desktop_apply_color(int i);
const char *desktop_color_name(int i);
int  desktop_color_by_name(const char *name);
int  desktop_focused(void);
int  desktop_sel_kind(void);
void desktop_alert(void);
int  desktop_flash_active(void);
int  desktop_drag_begin(struct Task *t, const char *abs_path);
int  desktop_drag_path(char *out, int max);

#endif
