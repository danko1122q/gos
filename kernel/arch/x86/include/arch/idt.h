#ifndef ARCH_IDT_H
#define ARCH_IDT_H
#include "lib/types.h"
#include "arch/gdt.h"

typedef struct Regs {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    u32 int_no, err;
    u32 eip, cs, eflags;
    u32 useresp, ss;
} Regs;

typedef void (*IsrHandler)(Regs *r);

#define REGS_SYSNO(r)  ((r)->eax)
#define REGS_ARG0(r)   ((r)->ebx)
#define REGS_ARG1(r)   ((r)->ecx)
#define REGS_ARG2(r)   ((r)->edx)
#define REGS_ARG3(r)   ((r)->esi)
#define REGS_ARG4(r)   ((r)->edi)
#define REGS_RET(r)    ((r)->eax)
#define REGS_PC(r)     ((r)->eip)
#define REGS_VEC(r)    ((r)->int_no)
#define REGS_ERR(r)    ((r)->err)
#define REGS_USER(r)   (((r)->cs & 3) != 0)

void idt_init(void);
void isr_register(int vec, IsrHandler h);
void isr_dispatch(Regs *r);

#endif
