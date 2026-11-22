#ifndef ARCH_IDT_H
#define ARCH_IDT_H
#include "lib/types.h"

typedef struct Regs {
    u32 r[13];
    u32 sp, lr;
    u32 pc;
    u32 cpsr;
    u32 int_no;
    u32 err;
    u32 addr;
    u32 pad0, pad1;
} Regs;

typedef void (*IsrHandler)(Regs *r);

#define REGS_SYSNO(r)  ((r)->r[7])
#define REGS_ARG0(r)   ((r)->r[0])
#define REGS_ARG1(r)   ((r)->r[1])
#define REGS_ARG2(r)   ((r)->r[2])
#define REGS_ARG3(r)   ((r)->r[3])
#define REGS_ARG4(r)   ((r)->r[4])
#define REGS_RET(r)    ((r)->r[0])
#define REGS_PC(r)     ((r)->pc)
#define REGS_VEC(r)    ((r)->int_no)
#define REGS_ERR(r)    ((r)->err)
#define REGS_USER(r)   (((r)->cpsr & 0x1f) == 0x10)

void isr_register(int vec, IsrHandler h);
void isr_dispatch(Regs *r);

#endif
