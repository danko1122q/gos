#ifndef SYS_CLIP_H
#define SYS_CLIP_H
#include "lib/types.h"

int clip_set(const void *data, u32 n);
u32 clip_len(void);
u32 clip_get(void *dst, u32 max);

#endif
