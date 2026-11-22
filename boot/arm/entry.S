#include "arch/frame.h"
        .syntax unified
        .arm
        .section .entry, "ax"
        .globl _start
_start:
        msr     cpsr_c, #(MODE_SVC | PSR_I | PSR_F)
        mov     r4, r2
        ldr     r0, =arm_vectors
        mov     r1, #0
        mov     r2, #16
1:      ldr     r3, [r0], #4
        str     r3, [r1], #4
        subs    r2, r2, #1
        bne     1b
        msr     cpsr_c, #(0x12 | PSR_I | PSR_F)
        ldr     sp, =irq_scratch
        msr     cpsr_c, #(0x17 | PSR_I | PSR_F)
        ldr     sp, =abt_scratch
        msr     cpsr_c, #(0x1B | PSR_I | PSR_F)
        ldr     sp, =und_scratch
        msr     cpsr_c, #(MODE_SVC | PSR_I | PSR_F)
        ldr     sp, =0x9F000
        ldr     r0, =__bss_start
        ldr     r1, =__bss_end
        mov     r2, #0
2:      cmp     r0, r1
        strlo   r2, [r0], #4
        blo     2b
        ldr     r0, =arm_atags
        str     r4, [r0]
        bl      kmain
3:      b       3b

        .globl arm_soft_reset
arm_soft_reset:
        mov     r4, r0
        msr     cpsr_c, #(MODE_SVC | PSR_I | PSR_F)
        mrc     p15, 0, r0, c1, c0, 0
        bic     r0, r0, #0x2000
        bic     r0, r0, #0x1
        mcr     p15, 0, r0, c1, c0, 0
        nop
        nop
        mov     r0, #0
        ldr     r1, =0x183
        mov     r2, r4
        ldr     pc, =_start
        .ltorg

        .section .bss
        .balign 8
        .globl irq_scratch, abt_scratch, und_scratch
irq_scratch: .space 32
abt_scratch: .space 32
und_scratch: .space 32
        .globl arm_atags
arm_atags:   .space 4
