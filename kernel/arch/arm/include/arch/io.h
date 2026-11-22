#ifndef ARCH_IO_H
#define ARCH_IO_H
#include "lib/types.h"
#include "arch/hw.h"

static inline void cli(void) { u32 t; __asm__ volatile("mrs %0, cpsr\n\torr %0, %0, #0x80\n\tmsr cpsr_c, %0" : "=&r"(t) :: "memory", "cc"); }
static inline void sti(void) { u32 t; __asm__ volatile("mrs %0, cpsr\n\tbic %0, %0, #0x80\n\tmsr cpsr_c, %0" : "=&r"(t) :: "memory", "cc"); }
static inline u32 irq_save(void) { u32 f, t; __asm__ volatile("mrs %0, cpsr\n\torr %1, %0, #0x80\n\tmsr cpsr_c, %1" : "=&r"(f), "=&r"(t) :: "memory", "cc"); return f; }
static inline void irq_restore(u32 f) { if (!(f & 0x80)) sti(); }
static inline void hlt(void) { __asm__ volatile("mcr p15, 0, %0, c7, c0, 4" :: "r"(0) : "memory"); }
static inline void cpu_wait_irq(void) { hlt(); sti(); }

#endif
