#ifndef GUI_MENU_H
#define GUI_MENU_H

enum { MENU_APPLE, MENU_FILE, MENU_EDIT, MENU_VIEW, MENU_SPECIAL, MENU_APP, MENU_COUNT };

enum {
    A_NONE = 0, A_ABOUT, A_SYNC, A_REBOOT, A_POWEROFF,
    A_NEW_TERM, A_NEW_FILES, A_NEW_EDIT, A_NEW_MON, A_NEW_PAINT, A_CLOSE,
    A_ZOOM, A_NEXT,
    A_DA_ALARM, A_DA_CALC, A_DA_CONTROL, A_DA_KEYCAPS, A_DA_NOTEPAD, A_DA_PUZZLE, A_DA_SCRAP, A_SHOW_CLIP,
    A_DK_NEWFOLDER, A_DK_OPEN, A_DK_INFO, A_DK_DUP, A_DK_TRASH,
    A_CLEANUP, A_EMPTY_TRASH, A_EMPTY_TRASH_OK,
    A_HIDE, A_HIDE_OTHERS, A_SHOW_ALL,
    A_WINCMD_FILE = 100,
    A_WINCMD_VIEW = 200,
    A_WINCMD_EDIT = 300,
    A_FOCUS = 1000
};

int  menu_current(void);
void menu_open(int m);
void menu_close(void);
int  menu_bar_hit(int x, int y);
int  menu_item_hit(int x, int y);
int  menu_item_action(int idx);
void menu_draw(int px, int py, const char *clock);

#endif
