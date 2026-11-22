#include "lib/timeconv.h"

static int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static const int mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

u32 dt_to_epoch(int y, int mo, int d, int h, int mi, int s) {
    u32 days = 0;
    for (int i = 1970; i < y; i++) days += is_leap(i) ? 366 : 365;
    for (int i = 1; i < mo; i++) days += mdays[i - 1] + (i == 2 && is_leap(y));
    days += d - 1;
    return days * 86400 + h * 3600 + mi * 60 + s;
}

void epoch_to_dt(u32 e, DateTime *t) {
    u32 days = e / 86400, rem = e % 86400;
    t->hour = rem / 3600; t->min = rem % 3600 / 60; t->sec = rem % 60;
    t->wday = (days + 4) % 7;
    int y = 1970;
    for (;;) { u32 yd = is_leap(y) ? 366 : 365; if (days < yd) break; days -= yd; y++; }
    int m = 0;
    for (;; m++) { u32 md = mdays[m] + (m == 1 && is_leap(y)); if (days < md) break; days -= md; }
    t->year = y; t->mon = m + 1; t->day = days + 1;
}
