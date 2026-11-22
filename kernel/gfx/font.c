#include "gfx/font.h"
#include "gfx/draw.h"
#include "lib/string.h"

#ifdef GOS_USERLAND
static const u8 *font_base;
#else
#include "arch/arch.h"
static const u8 *font_base = GOS_FONT8X16;
#endif
void font_set_base(const u8 *p) { font_base = p; }
const u8 *font_get_base(void) { return font_base; }

static void glyph(int x, int y, char ch, u8 fg, int bg) {
    const u8 *g = font_base + (u8)ch * 16;
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 8; c++) {
            if (g[r] & (0x80 >> c)) draw_px(x + c, y + r, fg);
            else if (bg >= 0) draw_px(x + c, y + r, (u8)bg);
        }
}

int text_width(const char *s) { return (int)strlen(s) * 8; }

void text_draw(int x, int y, const char *s, u8 fg, int bg) {
    for (; *s; s++, x += 8) glyph(x, y, *s, fg, bg);
}
void text_draw_bold(int x, int y, const char *s, u8 fg) {
    text_draw(x, y, s, fg, -1); text_draw(x + 1, y, s, fg, -1);
}
void text_draw_center(int cx, int y, const char *s, u8 fg, int bg) {
    text_draw(cx - (int)strlen(s) * 4, y, s, fg, bg);
}

static const UiFont *uif[3];
void font_set_ui(int face, const UiFont *f) { if (face == FACE_CHICAGO || face == FACE_GENEVA) uif[face] = f; }
const UiFont *font_get_ui(int face) { return (face == FACE_CHICAGO || face == FACE_GENEVA) ? uif[face] : NULL; }

static int gidx(char ch) {
    u8 c = (u8)ch;
    if (c == 0xFB) return 95;
    if (c == 0xF6) return 96;
    if (c == 0xF1) return 97;
    return (c >= 32 && c <= 126) ? c - 32 : '?' - 32;
}
static int is_bold(int face, const UiFont *f) { return (face & FS_BOLD) && f != uif[FACE_CHICAGO]; }

int ftext_width(int face, const char *s) {
    const UiFont *f = font_get_ui(face & FACE_MASK);
    if (!f) return (int)strlen(s) * 8;
    int w = 0, b = is_bold(face, f);
    for (; *s; s++) w += f->width[gidx(*s)] + b;
    return w;
}

static void uglyph(const UiFont *f, int g, int x, int y, u8 fg) {
    for (int r = 0; r < f->height; r++) {
        u8 v = f->bits[g][r];
        for (int c = 0; v; c++, v = (u8)(v << 1))
            if (v & 0x80) draw_px(x + c, y + r, fg);
    }
}

void ftext_draw(int face, int x, int y, const char *s, u8 fg, int bg) {
    const UiFont *f = font_get_ui(face & FACE_MASK);
    if (!f) { text_draw(x, y, s, fg, bg); if (face & FS_BOLD) text_draw(x + 1, y, s, fg, -1); return; }
    int b = is_bold(face, f), dy = (FONT_SLOT - f->height) / 2;
    for (; *s; s++) {
        int g = gidx(*s), adv = f->width[g] + b;
        if (bg >= 0) draw_rect(x, y, adv, FONT_SLOT, (u8)bg);
        uglyph(f, g, x, y + dy, fg);
        if (b) uglyph(f, g, x + 1, y + dy, fg);
        x += adv;
    }
}

void ftext_draw_center(int face, int cx, int y, const char *s, u8 fg, int bg) {
    ftext_draw(face, cx - ftext_width(face, s) / 2, y, s, fg, bg);
}

void ftext_fit(int face, const char *s, int maxw, char *out, int outsz) {
    if (outsz <= 0) return;
    if (ftext_width(face, s) <= maxw) { strlcpy(out, s, (u32)outsz); return; }
    int dots = ftext_width(face, ".."), n = 0, w = 0;
    while (s[n] && n < outsz - 3) {
        char one[2] = { s[n], 0 };
        if (w + ftext_width(face, one) + dots > maxw) break;
        w += ftext_width(face, one); n++;
    }
    memcpy(out, s, (size_t)n);
    out[n] = '.'; out[n + 1] = '.'; out[n + 2] = 0;
}
