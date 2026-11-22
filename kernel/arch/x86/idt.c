#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/panic.h"
#include "arch/io.h"
#include "proc/task.h"
#include "proc/exec.h"

typedef struct { u16 lo, sel; u8 zero, flags; u16 hi; } __attribute__((packed)) IdtEnt;
typedef struct { u16 size; u32 base; } __attribute__((packed)) IdtPtr;

extern u32 isr_stub_table[48];
extern void isr_stub_80(void);

static IdtEnt idt[256];
static IdtPtr ip;
static IsrHandler handlers[256];

static void gate(int v, u32 fn, u8 flags) {
    idt[v].lo = fn & 0xFFFF; idt[v].hi = fn >> 16;
    idt[v].sel = SEL_KCODE; idt[v].zero = 0; idt[v].flags = flags;
}

void idt_init(void) {
    for (int i = 0; i < 48; i++) gate(i, isr_stub_table[i], 0x8E);
    gate(0x80, (u32)isr_stub_80, 0xEF);
    ip.size = sizeof idt - 1; ip.base = (u32)idt;
    __asm__ volatile("lidt %0" :: "m"(ip));
}

void isr_register(int v, IsrHandler h) { if (v >= 0 && v < 256) handlers[v] = h; }

void isr_dispatch(Regs *r) {
    int v = (int)r->int_no;
    if (v >= 32 && v < 48) {
        int irq = v - 32;
        if (irq == 7 || irq == 15) {
            outb(irq == 7 ? 0x20 : 0xA0, 0x0B);
            if (!(inb(irq == 7 ? 0x20 : 0xA0) & 0x80)) { if (irq == 15) outb(0x20, 0x20); return; }
        }
        pic_eoi(irq);
    }
    if (handlers[v]) { handlers[v](r); return; }
    if (v < 32) {
        if (r->cs & 3) proc_fault(r, 0);
        else panic_exception(r);
    }
}
