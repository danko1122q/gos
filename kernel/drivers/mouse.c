#include "drivers/mouse.h"
#include "gui/event.h"
#include "sys/settings.h"

static int mx = 320, my = 240, btn;
static int bw = 640, bh = 480;
static int has_wheel;

void mouse_set_wheel(int on) { has_wheel = on; }

void mouse_set_bounds(int w, int h) { bw = w; bh = h; }
int  mouse_x(void) { return mx; }
int  mouse_y(void) { return my; }

void mouse_byte(u8 d) {
    static u8 pk[4]; static int pi;
    if (pi == 0 && !(d & 8)) return;
    pk[pi++] = d;
    if (pi < (has_wheel ? 4 : 3)) return;
    pi = 0;
    int z = 0;
    if (has_wheel) { z = pk[3] & 0x0F; if (z & 8) z -= 16; }
    int dx = pk[1], dy = pk[2];
    if (pk[0] & 0x10) dx -= 256;
    if (pk[0] & 0x20) dy -= 256;
    {
        static int rx, ry; int num, den;
        set_mouse_scale(&num, &den);
        int vx = dx * num + rx, vy = dy * num + ry;
        dx = vx / den; rx = vx - dx * den;
        dy = vy / den; ry = vy - dy * den;
    }
    mx += dx; my -= dy;
    if (mx < 0) mx = 0;
    if (mx > bw - 1) mx = bw - 1;
    if (my < 0) my = 0;
    if (my > bh - 1) my = bh - 1;
    int nb = pk[0] & 1;
    Event e = { .type = EV_MOUSE_MOVE, .x = mx, .y = my, .buttons = pk[0] & 7 };
    if (nb && !btn) e.type = EV_MOUSE_DOWN;
    else if (!nb && btn) e.type = EV_MOUSE_UP;
    btn = nb;
    event_push(&e);
    if (z) { Event w = { .type = EV_MOUSE_WHEEL, .x = mx, .y = my, .buttons = pk[0] & 7, .a = z }; event_push(&w); }
}
