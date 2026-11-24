#ifndef ARCH_ARCH_H
#define ARCH_ARCH_H
#include "lib/types.h"

#define GOS_ARCH_NAME    "i386"
#define GOS_ARCH_DESC    "i386 (32-bit protected mode)"
#define GOS_ELF_MACHINE  3
#define PMM_MAX_RAM      0x20000000u
#define VMM_PD_FRAMES    1u
#define GOS_FONT8X16     ((const u8 *)0x90000)
#define VIDEO_NBUF       1

void arch_early_init(void);
int  arch_cpuinfo(char *buf, int max);

#endif
