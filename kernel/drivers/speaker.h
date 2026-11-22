#ifndef DRV_SPEAKER_H
#define DRV_SPEAKER_H
#include "lib/types.h"

void speaker_beep(u32 hz, u32 ms);
void speaker_tick(void);

#endif
