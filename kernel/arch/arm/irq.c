#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/hw.h"
#include "arch/io.h"
#include "arch/frame.h"
#include "arch/panic.h"
#include "mem/vmm.h"
#include "proc/task.h"
#include "proc/exec.h"

_Static_assert(__builtin_offsetof(Regs, sp) == R_SP && __builtin_offsetof(Regs, pc) == R_PC && __builtin_offsetof(Regs, cpsr) == R_CPSR &&
               __builtin_offsetof(Regs, int_no) == R_VEC && __builtin_offsetof(Regs, err) == R_ERR && __builtin_offsetof(Regs, addr) == R_ADDR &&
               sizeof(Regs) == REGS_SIZE, "Regs doesn't match arch/frame.h");

static IsrHandler handlers[256];
void isr_register(int v, IsrHandler h) { if (v >= 0 && v < 256) handlers[v] = h; }

void isr_dispatch(Regs *r) {
    u32 v = r->int_no;
    if (v >= 32 && v < 48) pic_eoi((int)v - 32);
    if (v < 256 && handlers[v]) { handlers[v](r); return; }
    if (v < 32) {
        if (REGS_USER(r)) proc_fault(r, r->addr);
        else panic_exception(r);
    }
}

#define VIC_STATUS   0x00
#define VIC_SELECT   0x0C
#define VIC_ENABLE   0x10
#define VIC_ENCLR    0x14
#define SIC_STATUS   0x00
#define SIC_ENSET    0x08
#define SIC_ENCLR    0x0C
#define TIMER_INTCLR 0x0C

void pic_init(void) {
    wr32(VIC_BASE + VIC_ENCLR, 0xFFFFFFFFu);
    wr32(VIC_BASE + VIC_SELECT, 0);
    wr32(SIC_BASE + SIC_ENCLR, 0xFFFFFFFFu);
}

void pic_unmask(int irq) {
    switch (irq) {
    case 0:  wr32(VIC_BASE + VIC_ENABLE, 1u << VIC_IRQ_TIMER01); break;
    case 1:  wr32(SIC_BASE + SIC_ENSET, 1u << SIC_IRQ_KMI0); wr32(VIC_BASE + VIC_ENABLE, 1u << VIC_IRQ_SIC); break;
    case 12: wr32(SIC_BASE + SIC_ENSET, 1u << SIC_IRQ_KMI1); wr32(VIC_BASE + VIC_ENABLE, 1u << VIC_IRQ_SIC); break;
    }
}
void pic_mask(int irq) {
    switch (irq) {
    case 0:  wr32(VIC_BASE + VIC_ENCLR, 1u << VIC_IRQ_TIMER01); break;
    case 1:  wr32(SIC_BASE + SIC_ENCLR, 1u << SIC_IRQ_KMI0); break;
    case 12: wr32(SIC_BASE + SIC_ENCLR, 1u << SIC_IRQ_KMI1); break;
    }
}
void pic_eoi(int irq) { if (irq == 0) wr32(TIMER0_BASE + TIMER_INTCLR, 1); }

static void arm_irq(Regs *r) {
    for (int n = 0; n < 8; n++) {
        u32 st = rd32(VIC_BASE + VIC_STATUS), known = 0;
        if (!st) return;
        if (st & (1u << VIC_IRQ_TIMER01)) { known |= 1u << VIC_IRQ_TIMER01; r->int_no = 32; isr_dispatch(r); }
        if (st & (1u << VIC_IRQ_SIC)) {
            u32 s = rd32(SIC_BASE + SIC_STATUS);
            known |= 1u << VIC_IRQ_SIC;
            if (s & (1u << SIC_IRQ_KMI0)) { r->int_no = 33; isr_dispatch(r); }
            if (s & (1u << SIC_IRQ_KMI1)) { r->int_no = 44; isr_dispatch(r); }
            if (!(s & ((1u << SIC_IRQ_KMI0) | (1u << SIC_IRQ_KMI1)))) wr32(SIC_BASE + SIC_ENCLR, s);
        }
        if (st & ~known) wr32(VIC_BASE + VIC_ENCLR, st & ~known);
    }
}

static inline u32 cp15_fsr(void) { u32 v; __asm__ volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(v)); return v; }
static inline u32 cp15_far(void) { u32 v; __asm__ volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(v)); return v; }

void arm_trap(Regs *r) {
    switch (r->int_no) {
    case VEC_IRQ:
        arm_irq(r);
        break;
    case VEC_SWI:
        r->int_no = 0x80;
        if (!(r->cpsr & PSR_I)) sti();
        isr_dispatch(r);
        break;
    case VEC_UND:
        r->int_no = 6;
        r->addr = r->pc;
        isr_dispatch(r);
        break;
    case VEC_DABT: {
        u32 fsr = cp15_fsr(), far = cp15_far(), st = fsr & 0xF;
        r->addr = far;
        r->err = ((st == 0x9 || st == 0xB || st == 0xD || st == 0xF) ? 1u : 0u) | (fsr << 8);
        r->int_no = (st == 0x1 || st == 0x3) ? 17 : 14;
        isr_dispatch(r);
        break;
    }
    case VEC_PABT:
        r->addr = r->pc;
        r->err = (vmm_walk(vmm_current(), r->pc) & PTE_P) ? 1u : 0u;
        r->int_no = 14;
        isr_dispatch(r);
        break;
    default:
        panic_exception(r);
    }
}
