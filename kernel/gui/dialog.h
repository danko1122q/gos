#ifndef GUI_DIALOG_H
#define GUI_DIALOG_H
#include "lib/types.h"

void dialog_show_about(void);
void dialog_msg(const char *title, const char *l1, const char *l2);
u32  dialog_ask(const char *title, const char *l1, const char *l2, const char *ok, int act);
int  dialog_ask_result(u32 token);
int  dialog_click(int x, int y);
int  dialog_key(int key);
void dialog_close(void);
int  dialog_visible(void);
void dialog_draw(void);

#endif
