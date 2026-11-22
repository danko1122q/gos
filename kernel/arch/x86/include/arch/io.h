#ifndef ARCH_IO_H
#define ARCH_IO_H
#include "lib/types.h"

static inline void outb(u16 p, u8 v)  { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline u8  inb(u16 p)  { u8 v;  __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline u16 inw(u16 p)  { u16 v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void cli(void) { __asm__ volatile("cli" ::: "memory"); }
static inline void sti(void) { __asm__ volatile("sti" ::: "memory"); }
static inline void insw(u16 p, void *d, u32 n)        { __asm__ volatile("rep insw" : "+D"(d), "+c"(n) : "d"(p) : "memory"); }
static inline void outsw(u16 p, const void *s, u32 n) { __asm__ volatile("rep outsw" : "+S"(s), "+c"(n) : "d"(p) : "memory"); }
static inline u32 irq_save(void) { u32 f; __asm__ volatile("pushf; pop %0; cli" : "=r"(f) :: "memory"); return f; }
static inline void irq_restore(u32 f) { if (f & 0x200) sti(); }
static inline void hlt(void) { __asm__ volatile("hlt"); }
static inline void cpu_wait_irq(void) { __asm__ volatile("sti; hlt" ::: "memory"); }

#endif
