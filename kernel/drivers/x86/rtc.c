#include "drivers/rtc.h"
#include "arch/io.h"
#include "arch/timer.h"
#include "gui/event.h"
#include "lib/errno.h"

static int last_h = -1, last_m = -1;
static u32 base_epoch, base_up;

static int cmos(int r) { outb(0x70, r); return inb(0x71); }
static int bcd(int v, int b) { return (b & 4) ? v : (v & 15) + (v >> 4) * 10; }

void rtc_init(void) {
    while (cmos(0x0A) & 0x80) ;
    int b = cmos(0x0B);
    int s = bcd(cmos(0), b), mi = bcd(cmos(2), b), h = cmos(4);
    int pm = h & 0x80; h &= 0x7F; h = bcd(h, b);
    if (!(b & 2) && pm) h = (h % 12) + 12;
    int d = bcd(cmos(7), b), mo = bcd(cmos(8), b), y = 2000 + bcd(cmos(9), b);
    base_epoch = dt_to_epoch(y, mo, d, h, mi, s);
    base_up = uptime_sec();
}

static void cmos_w(int r, int v) { outb(0x70, r); outb(0x71, v); }
static int to_bcd(int v, int b) { return (b & 4) ? v : ((v / 10) << 4) | (v % 10); }

int rtc_set(u32 epoch) {
    DateTime t; epoch_to_dt(epoch, &t);
    if (t.year < 2000 || t.year > 2099) return -EINVAL;
    while (cmos(0x0A) & 0x80) ;
    int b = cmos(0x0B), h = t.hour;
    u32 f = irq_save();
    cmos_w(0x0B, b | 0x80);
    if (!(b & 2)) { int pm = h >= 12; h %= 12; if (!h) h = 12; h = to_bcd(h, b) | (pm ? 0x80 : 0); }
    else h = to_bcd(h, b);
    cmos_w(0, to_bcd(t.sec, b)); cmos_w(2, to_bcd(t.min, b)); cmos_w(4, h);
    cmos_w(6, to_bcd(t.wday + 1, b)); cmos_w(7, to_bcd(t.day, b)); cmos_w(8, to_bcd(t.mon, b)); cmos_w(9, to_bcd(t.year % 100, b));
    cmos_w(0x0B, b & ~0x80);
    base_epoch = epoch; base_up = uptime_sec(); last_h = -1;
    irq_restore(f);
    return 0;
}

u32 time_now(void) { return base_epoch + (uptime_sec() - base_up); }

void rtc_poll(void) {
    DateTime t; epoch_to_dt(time_now(), &t);
    if (t.hour == last_h && t.min == last_m) return;
    last_h = t.hour; last_m = t.min;
    Event e = { .type = EV_CLOCK, .a = t.hour, .b = t.min };
    event_push(&e);
}
