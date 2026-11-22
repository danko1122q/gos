#ifndef ARCH_HW_H
#define ARCH_HW_H
#include "lib/types.h"

#define SYSCTL_BASE   0x10000000u
#define SIC_BASE      0x10003000u
#define MMCI_BASE     0x10005000u
#define KMI0_BASE     0x10006000u
#define KMI1_BASE     0x10007000u
#define CLCD_BASE     0x10120000u
#define VIC_BASE      0x10140000u
#define TIMER0_BASE   0x101E2000u
#define RTC_BASE      0x101E8000u
#define UART0_BASE    0x101F1000u
#define PERI_BASE     0x10000000u
#define PERI_END      0x10200000u

#define VIC_IRQ_TIMER01  4
#define VIC_IRQ_SIC      31
#define SIC_IRQ_KMI0     3
#define SIC_IRQ_KMI1     4

static inline u32  rd32(u32 a)         { return *(volatile u32 *)a; }
static inline void wr32(u32 a, u32 v)  { *(volatile u32 *)a = v; }

#endif
