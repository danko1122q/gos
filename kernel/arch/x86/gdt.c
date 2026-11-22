#include "arch/gdt.h"
#include "arch/idt.h"
#include "lib/string.h"

typedef struct { u16 lim, base_lo; u8 base_mid, access, gran, base_hi; } __attribute__((packed)) GdtEnt;
typedef struct { u16 size; u32 base; } __attribute__((packed)) GdtPtr;

typedef struct {
    u32 prev, esp0, ss0, esp1, ss1, esp2, ss2, cr3, eip, eflags;
    u32 eax, ecx, edx, ebx, esp, ebp, esi, edi;
    u32 es, cs, ss, ds, fs, gs, ldt;
    u16 trap, iomap;
} __attribute__((packed)) Tss;

static GdtEnt gdt[6];
static GdtPtr gp;
static Tss tss;

static void set(int i, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[i].lim = limit & 0xFFFF; gdt[i].base_lo = base & 0xFFFF; gdt[i].base_mid = (base >> 16) & 0xFF;
    gdt[i].base_hi = base >> 24; gdt[i].access = access;
    gdt[i].gran = (gran & 0xF0) | ((limit >> 16) & 0x0F);
}

void tss_set_esp0(u32 esp0) { tss.esp0 = esp0; }

void gdt_init(void) {
    memset(gdt, 0, sizeof gdt);
    set(1, 0, 0xFFFFF, 0x9A, 0xC0);
    set(2, 0, 0xFFFFF, 0x92, 0xC0);
    set(3, 0, 0xFFFFF, 0xFA, 0xC0);
    set(4, 0, 0xFFFFF, 0xF2, 0xC0);
    memset(&tss, 0, sizeof tss);
    tss.ss0 = SEL_KDATA; tss.esp0 = 0x9F000; tss.iomap = sizeof tss;
    set(5, (u32)&tss, sizeof tss - 1, 0x89, 0x00);
    gp.size = sizeof gdt - 1; gp.base = (u32)gdt;
    __asm__ volatile(
        "lgdt %0\n"
        "ljmp $0x08, $1f\n"
        "1: mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n mov %%ax, %%es\n mov %%ax, %%fs\n mov %%ax, %%gs\n mov %%ax, %%ss\n"
        "mov $0x28, %%ax\n ltr %%ax\n"
        :: "m"(gp) : "eax", "memory");
}
