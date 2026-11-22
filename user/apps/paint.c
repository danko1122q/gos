#include "uikit.h"

#define CW 320
#define CH 240
#define CX 58
#define CY 4
enum { T_PEN, T_ERASE, T_LINE, T_RECT, T_FRECT, T_OVAL, T_FOVAL, T_FILL, NT };
static const char *tname[NT] = { "Pencil", "Eraser", "Line", "Rect", "Filled Rect", "Oval", "Filled Oval", "Fill" };
static const u8 rgb[12][3] = { {0,0,0},{255,255,255},{0,120,72},{176,204,112},{230,20,20},{150,150,150},{90,90,90},{30,60,170},{40,170,70},{240,220,60},{40,190,200},{210,210,210} };

static int sn_(int a) { static const int t[40] = {0,16,31,45,59,71,81,89,95,99,100,99,95,89,81,71,59,45,31,16,0,-16,-31,-45,-59,-71,-81,-89,-95,-99,-100,-99,-95,-89,-81,-71,-59,-45,-31,-16}; return t[a % 40]; }
static u8 cv[CW * CH], snap[CW * CH], undo[CW * CH];
static int tool = T_PEN, color = C_BLK, size = 1, dirty = 1, quit, painting, sx, sy, lx, ly, modified, has_undo;
static char path[128] = "/home/user/Documents/untitled.bmp";
static int prompt; static char pin[100]; static int plen;

static void put(int x, int y, u8 c) { if (x >= 0 && y >= 0 && x < CW && y < CH) cv[y * CW + x] = c; }
static void dot(int x, int y, u8 c) { int h = size / 2; for (int j = 0; j < size; j++) for (int i = 0; i < size; i++) put(x - h + i, y - h + j, c); }
static void line(int x0, int y0, int x1, int y1, u8 c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx_ = x0 < x1 ? 1 : -1, sy_ = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) { dot(x0, y0, c); if (x0 == x1 && y0 == y1) break; int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx_; } if (e2 <= dx) { e += dx; y0 += sy_; } }
}
static void rect(int x0, int y0, int x1, int y1, int fill, u8 c) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; } if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (fill) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) put(x, y, c); return; }
    line(x0, y0, x1, y0, c); line(x0, y1, x1, y1, c); line(x0, y0, x0, y1, c); line(x1, y0, x1, y1, c);
}
static int inside(int x, int y, int a, int b) { if (a < 1 || b < 1) return x == 0 && y == 0; return (long)x * x * b * b + (long)y * y * a * a <= (long)a * a * b * b; }
static void oval(int x0, int y0, int x1, int y1, int fill, u8 c) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; } if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, a = (x1 - x0) / 2, b = (y1 - y0) / 2;
    for (int y = -b; y <= b; y++) for (int x = -a; x <= a; x++) {
        if (!inside(x, y, a, b)) continue;
        if (fill || !inside(x, y, a - size, b - size)) put(cx + x, cy + y, c);
    }
}
static void flood(int x, int y, u8 c) {
    if (x < 0 || y < 0 || x >= CW || y >= CH) return;
    u8 old = cv[y * CW + x]; if (old == c) return;
    static short stx[8192], sty[8192]; int sp = 0;
    stx[sp] = (short)x; sty[sp++] = (short)y;
    while (sp) {
        int px = stx[--sp], py = sty[sp];
        if (cv[py * CW + px] != old) continue;
        int l = px, r = px;
        while (l > 0 && cv[py * CW + l - 1] == old) l--;
        while (r < CW - 1 && cv[py * CW + r + 1] == old) r++;
        for (int i = l; i <= r; i++) cv[py * CW + i] = c;
        for (int d = -1; d <= 1; d += 2) {
            int ny = py + d; if (ny < 0 || ny >= CH) continue;
            int in = 0;
            for (int i = l; i <= r; i++) {
                int m = cv[ny * CW + i] == old;
                if (m && !in && sp < 8190) { stx[sp] = (short)i; sty[sp++] = (short)ny; }
                in = m;
            }
        }
    }
}

static int nearest(int r, int g, int b) {
    int best = 0, bd = 1 << 30;
    for (int i = 0; i < 12; i++) { int dr = r - rgb[i][0], dg = g - rgb[i][1], db = b - rgb[i][2], d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; best = i; } }
    return best;
}
static void le32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static u32 rd32(const u8 *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static int save_bmp(const char *p) {
    u32 off = 14 + 40 + 1024, sz = off + CW * CH;
    u8 *b = kzalloc(sz); if (!b) return -ENOMEM;
    b[0] = 'B'; b[1] = 'M'; le32(b + 2, sz); le32(b + 10, off);
    le32(b + 14, 40); le32(b + 18, CW); le32(b + 22, CH); b[26] = 1; b[28] = 8; le32(b + 34, CW * CH); le32(b + 46, 256);
    for (int i = 0; i < 12; i++) { b[54 + i * 4] = rgb[i][2]; b[55 + i * 4] = rgb[i][1]; b[56 + i * 4] = rgb[i][0]; }
    for (int y = 0; y < CH; y++) memcpy(b + off + (CH - 1 - y) * CW, cv + y * CW, CW);
    gos_mkdir("/home/user/Documents");
    int r = gfs_write(p, b, sz, 0); kfree(b); return r;
}
static int open_bmp(const char *p) {
    char *d; u32 l; int r = gfs_read(p, &d, &l);
    if (r) return r;
    const u8 *b = (const u8 *)d; int ok = -EINVAL;
    if (l > 54 && b[0] == 'B' && b[1] == 'M') {
        u32 off = rd32(b + 10); int w = (int)rd32(b + 18), h = (int)rd32(b + 22), bpp = b[28] | b[29] << 8; int flip = h > 0; if (h < 0) h = -h;
        if (w > 0 && h > 0 && (bpp == 8 || bpp == 24) && !rd32(b + 30)) {
            u32 row = bpp == 8 ? (u32)((w + 3) & ~3) : (u32)((w * 3 + 3) & ~3), pal = 14 + rd32(b + 14);
            if ((u64)off + (u64)row * (u32)h <= l) {
                memset(cv, C_WHT, sizeof cv);
                for (int y = 0; y < h && y < CH; y++) for (int x = 0; x < w && x < CW; x++) {
                    const u8 *s = b + off + row * (u32)(flip ? h - 1 - y : y);
                    if (bpp == 8) { const u8 *q = b + pal + 4 * s[x]; if (pal + 4 * (u32)s[x] + 3 < l) cv[y * CW + x] = (u8)nearest(q[2], q[1], q[0]); }
                    else cv[y * CW + x] = (u8)nearest(s[x * 3 + 2], s[x * 3 + 1], s[x * 3]);
                }
                ok = 0;
            }
        }
    }
    kfree(d); return ok;
}

static void draw(Win *w) {
    win_begin(w); draw_rect(0, 0, w->cw, w->ch, C_WIN);
    for (int i = 0; i < NT; i++) {
        int x = 4 + (i % 2) * 26, y = 4 + (i / 2) * 28;
        draw_box(x, y, 24, 26, i == tool ? C_BLK : C_WHT, C_BLK);
        u8 f = i == tool ? C_WHT : C_BLK;
        switch (i) {
        case T_PEN: draw_rect(x + 6, y + 17, 3, 3, f); for (int k = 0; k < 10; k++) draw_px(x + 8 + k, y + 16 - k, f); break;
        case T_ERASE: draw_box(x + 5, y + 9, 14, 8, i == tool ? C_BLK : C_WHT, f); break;
        case T_LINE: for (int k = 0; k < 14; k++) draw_px(x + 5 + k, y + 18 - k, f); break;
        case T_RECT: draw_frame(x + 5, y + 7, 14, 12, f); break;
        case T_FRECT: draw_rect(x + 5, y + 7, 14, 12, f); break;
        case T_OVAL: for (int a = 0; a < 40; a++) { draw_px(x + 12 + (sn_(a) * 7) / 100, y + 13 - (sn_((a + 10) % 40) * 5) / 100, f); } break;
        case T_FOVAL: for (int yy = -5; yy <= 5; yy++) for (int xx = -7; xx <= 7; xx++) if (xx * xx * 25 + yy * yy * 49 <= 1225) draw_px(x + 12 + xx, y + 13 + yy, f); break;
        case T_FILL: draw_rect(x + 6, y + 8, 12, 10, f); draw_rect(x + 18, y + 12, 3, 3, f); break;
        }
    }
    for (int s = 0; s < 3; s++) { int x = 4 + s * 17, y = 4 + 4 * 28 + 4; int sz = s == 0 ? 1 : s == 1 ? 3 : 6; draw_box(x, y, 15, 15, size == sz ? C_BLK : C_WHT, C_BLK); draw_rect(x + 7 - sz / 2, y + 7 - sz / 2, sz, sz, size == sz ? C_WHT : C_BLK); }
    draw_frame(CX - 1, CY - 1, CW + 2, CH + 2, C_BLK);
    for (int y = 0; y < CH; y++) memcpy(w->surf + (CY + y) * GOS_SURF_W + CX, cv + y * CW, CW);
    for (int i = 0; i < 12; i++) { int x = CX + i * 26, y = CY + CH + 6; draw_box(x, y, 24, 18, i, i == color ? C_RED : C_BLK); if (i == color) draw_frame(x - 1, y - 1, 26, 20, C_RED); }
    if (prompt) { draw_rect(0, w->ch - 22, w->cw, 22, C_WHT); draw_rect(0, w->ch - 22, w->cw, 1, C_BLK); char t[140]; ksnprintf(t, sizeof t, "%s%s_", prompt == 1 ? "Save as: " : "Open BMP: ", pin); ftext_draw(FACE_GENEVA, 4, w->ch - 20, t, C_BLK, -1); }
    char b[80]; ksnprintf(b, sizeof b, "%s%s  [%s]", modified ? "*" : "", path, tname[tool]); win_set_info(w, b);
    win_present(w);
}

static void commit_undo(void) { memcpy(undo, cv, sizeof cv); has_undo = 1; }
static int inside_cv(int x, int y) { return x >= 0 && y >= 0 && x < CW && y < CH; }

static void mouse(int ev, int x, int y) {
    x -= CX; y -= CY;
    if (ev == GE_MOUSE_DOWN) {
        if (x < -4 && y < 4 + 4 * 28 && x >= -54) { int i = ((x + 54) / 26) + ((y - 4) / 28) * 2; if (i >= 0 && i < NT) tool = i; dirty = 1; return; }
        if (x < -4 && y >= 4 + 4 * 28 + 4 && y < 4 + 4 * 28 + 19) { int s = (x + 54) / 17; size = s == 0 ? 1 : s == 1 ? 3 : 6; dirty = 1; return; }
        if (y >= CH + 6 && y < CH + 24 && x >= 0) { int i = x / 26; if (i < 12) color = i; dirty = 1; return; }
        if (!inside_cv(x, y)) return;
        commit_undo(); painting = 1; sx = lx = x; sy = ly = y; modified = 1;
        memcpy(snap, cv, sizeof cv);
        u8 c = tool == T_ERASE ? C_WHT : (u8)color;
        if (tool == T_PEN || tool == T_ERASE) dot(x, y, c);
        else if (tool == T_FILL) { flood(x, y, (u8)color); painting = 0; }
        dirty = 1;
    } else if (ev == GE_MOUSE_MOVE && painting) {
        u8 c = tool == T_ERASE ? C_WHT : (u8)color;
        if (tool == T_PEN || tool == T_ERASE) { line(lx, ly, x, y, c); lx = x; ly = y; }
        else { memcpy(cv, snap, sizeof cv); if (tool == T_LINE) line(sx, sy, x, y, c); else if (tool <= T_FRECT) rect(sx, sy, x, y, tool == T_FRECT, c); else oval(sx, sy, x, y, tool == T_FOVAL, c); }
        dirty = 1;
    } else if (ev == GE_MOUSE_UP) painting = 0;
}

static void do_save(const char *p) { int r = save_bmp(p); if (r < 0) win_msgbox("Paint", "Save failed.", gos_strerror(r)); else { strlcpy(path, p, sizeof path); modified = 0; } }
static void do_open(const char *p) { commit_undo(); int r = open_bmp(p); if (r < 0) win_msgbox("Paint", "Can't open this BMP.", "Supports uncompressed 8-bit / 24-bit BMP."); else { strlcpy(path, p, sizeof path); modified = 0; } }

int main(int argc, char **argv) {
    static Win w; GEvent e;
    if (win_create(&w, "Paint", -1, 0, 408, 336) < 0) return 1;
    win_set_minsize(&w, 408, 336);
    win_set_menu(&w, "New\nOpen BMP...\nSave\nSave As...\nClear", "");
    win_set_edit_menu(&w, "Undo");
    memset(cv, C_WHT, sizeof cv);
    if (argc > 1 && argv[1][0]) do_open(argv[1]);
    while (!quit) {
        if (dirty) { draw(&w); dirty = 0; }
        if (win_next(&w, &e, -1) <= 0) break;
        do {
            switch (e.type) {
            case GE_MOUSE_DOWN: case GE_MOUSE_MOVE: case GE_MOUSE_UP: if (!prompt) mouse(e.type, e.a, e.b); break;
            case GE_KEY:
                if (prompt) {
                    if (e.a == '\n') { int p = prompt; prompt = 0; if (plen) { if (p == 1) do_save(pin); else do_open(pin); } }
                    else if (e.a == 27) prompt = 0;
                    else if (e.a == '\b') { if (plen) pin[--plen] = 0; }
                    else if (e.a >= 32 && e.a < 127 && plen < 98) { pin[plen++] = (char)e.a; pin[plen] = 0; }
                    dirty = 1;
                } else if ((e.b & MOD_CTRL) && e.a == 'z') { if (has_undo) { memcpy(cv, undo, sizeof cv); dirty = 1; } }
                else if ((e.b & MOD_CTRL) && e.a == 's') { do_save(path); dirty = 1; }
                break;
            case GE_CMD:
                if (e.a == 2) { if (has_undo) memcpy(cv, undo, sizeof cv); }
                else switch (e.b) {
                case 0: commit_undo(); memset(cv, C_WHT, sizeof cv); modified = 0; break;
                case 1: prompt = 2; strlcpy(pin, path, sizeof pin); plen = (int)strlen(pin); break;
                case 2: do_save(path); break;
                case 3: prompt = 1; strlcpy(pin, path, sizeof pin); plen = (int)strlen(pin); break;
                case 4: commit_undo(); memset(cv, C_WHT, sizeof cv); modified = 1; break;
                }
                dirty = 1; break;
            case GE_RESIZE: dirty = 1; break;
            case GE_CLOSE:
                if (modified && !win_confirm("Paint", "The picture hasn't been saved.", "Close without saving?", "Close")) break;
                quit = 1; break;
            default: break;
            }
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    win_close(&w);
    return 0;
}
