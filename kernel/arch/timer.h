#ifndef ARCH_TIMER_H
#define ARCH_TIMER_H
#include "lib/types.h"

#define HZ 100

void timer_init(void);
u32  timer_ticks(void);
u32  uptime_sec(void);
u32  timer_ms_now(void);

#endif
