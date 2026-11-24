#ifndef ARCH_ARCH_H
#define ARCH_ARCH_H
#include "lib/types.h"

#define GOS_ARCH_NAME    "arm32"
#define GOS_ARCH_DESC    "ARM32 (ARMv5TE, ARM926EJ-S, Versatile/PB)"
#define GOS_ELF_MACHINE  40
#define PMM_MAX_RAM      0x10000000u
#define VMM_PD_FRAMES    4u
#define VIDEO_NBUF       2

extern const u8 font8x16[256 * 16];
#define GOS_FONT8X16     font8x16

void arch_early_init(void);
int  arch_cpuinfo(char *buf, int max);

#endif
