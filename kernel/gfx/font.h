#ifndef GFX_FONT_H
#define GFX_FONT_H
#include "lib/types.h"

void text_draw(int x, int y, const char *s, u8 fg, int bg);
void text_draw_bold(int x, int y, const char *s, u8 fg);
void text_draw_center(int cx, int y, const char *s, u8 fg, int bg);
int  text_width(const char *s);
void font_set_base(const u8 *p);
const u8 *font_get_base(void);

#define UIF_NGLYPH 98
#define UIF_H      12
#define FONT_SLOT  16
typedef struct {
    u8 height, ascent, nglyph, pad;
    u8 width[UIF_NGLYPH];
    u8 pad2[3];
    u8 bits[UIF_NGLYPH][UIF_H];
} UiFont;

enum { FACE_MONO = 0, FACE_CHICAGO = 1, FACE_GENEVA = 2, FACE_MASK = 0xFF, FS_BOLD = 0x100 };

void font_set_ui(int face, const UiFont *f);
const UiFont *font_get_ui(int face);
void font_init(void);

int  ftext_width(int face, const char *s);
void ftext_draw(int face, int x, int y, const char *s, u8 fg, int bg);
void ftext_draw_center(int face, int cx, int y, const char *s, u8 fg, int bg);
void ftext_fit(int face, const char *s, int maxw, char *out, int outsz);

#endif
