#ifndef GFX_ICONS_H
#define GFX_ICONS_H

typedef enum {
    ICON_FILE = 0, ICON_TEXT, ICON_FOLDER, ICON_PROGRAM,
    ICON_DRIVE, ICON_FLOPPY, ICON_TRASH, ICON_TERM, ICON_MONITOR, ICON_APP, ICON_TRASH_FULL
} IconType;

void icon_draw(int x, int y, int type);
void icon_draw_small(int x, int y, int type);
void icon_draw_sel(int x, int y, int type, int sel);
void icon_draw_small_sel(int x, int y, int type, int sel);

#endif
