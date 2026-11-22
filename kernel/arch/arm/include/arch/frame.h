#ifndef ARCH_FRAME_H
#define ARCH_FRAME_H
#define R_R0      0
#define R_SP      52
#define R_LR      56
#define R_PC      60
#define R_CPSR    64
#define R_VEC     68
#define R_ERR     72
#define R_ADDR    76
#define REGS_SIZE 88

#define MODE_USR  0x10
#define MODE_SVC  0x13
#define PSR_I     0x80
#define PSR_F     0x40

#define VEC_UND   1
#define VEC_SWI   2
#define VEC_PABT  3
#define VEC_DABT  4
#define VEC_IRQ   5
#endif
