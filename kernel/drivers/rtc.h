#ifndef DRV_RTC_H
#define DRV_RTC_H
#include "lib/types.h"
#include "lib/timeconv.h"

void rtc_init(void);
void rtc_poll(void);
int  rtc_set(u32 epoch);
u32  time_now(void);

#endif
