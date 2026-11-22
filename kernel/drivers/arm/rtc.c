#include "drivers/rtc.h"
#include "arch/hw.h"
#include "gui/event.h"
#include "lib/errno.h"

#define RTCDR 0x00
#define RTCLR 0x08
#define RTCCR 0x0C

static int last_h = -1, last_m = -1;

void rtc_init(void) { wr32(RTC_BASE + RTCCR, 1); }

int rtc_set(u32 epoch) {
    DateTime t; epoch_to_dt(epoch, &t);
    if (t.year < 2000 || t.year > 2099) return -EINVAL;
    wr32(RTC_BASE + RTCLR, epoch);
    last_h = -1;
    return 0;
}

u32 time_now(void) { return rd32(RTC_BASE + RTCDR); }

void rtc_poll(void) {
    DateTime t; epoch_to_dt(time_now(), &t);
    if (t.hour == last_h && t.min == last_m) return;
    last_h = t.hour; last_m = t.min;
    Event e = { .type = EV_CLOCK, .a = t.hour, .b = t.min };
    event_push(&e);
}
