#ifndef LIB_TIMECONV_H
#define LIB_TIMECONV_H
#include "lib/types.h"

typedef struct { int year, mon, day, hour, min, sec, wday; } DateTime;

void epoch_to_dt(u32 epoch, DateTime *dt);
u32  dt_to_epoch(int y, int mo, int d, int h, int mi, int s);

#endif
