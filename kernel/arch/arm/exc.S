#include "arch/frame.h"
        .syntax unified
        .arm

        .section .vectors, "ax"
        .globl arm_vectors
arm_vectors:
        ldr     pc, [pc, #24]
        ldr     pc, [pc, #24]
        ldr     pc, [pc, #24]
        ldr     pc, [pc, #24]
        ldr     pc, [pc, #24]
        ldr     pc, [pc, #24]
        ldr     pc, [pc, #24]
        ldr     pc, [pc, #24]
        .word   exc_hang, und_stub, swi_entry, pabt_stub, dabt_stub, exc_hang, irq_stub, exc_hang
        .space  4096 - 64
        .text

exc_hang:
        b       exc_hang

        .macro MODE_STUB name, adj, vec
\name:
        sub     lr, lr, #\adj
        stmia   sp, {r0, lr}
        mrs     r0, spsr
        str     r0, [sp, #8]
        mov     r0, #\vec
        str     r0, [sp, #12]
        mov     r0, sp
        msr     cpsr_c, #(MODE_SVC | PSR_I | PSR_F)
        b       exc_common
        .endm

        MODE_STUB irq_stub,  4, VEC_IRQ
        MODE_STUB und_stub,  4, VEC_UND
        MODE_STUB pabt_stub, 4, VEC_PABT
        MODE_STUB dabt_stub, 8, VEC_DABT

exc_common:
        str     sp, [r0, #16]
        bic     sp, sp, #7
        sub     sp, sp, #REGS_SIZE
        stmib   sp, {r1-r12}
        ldmia   r0, {r1-r4}
        ldr     r5, [r0, #16]
        str     r1, [sp, #R_R0]
        str     r2, [sp, #R_PC]
        str     r3, [sp, #R_CPSR]
        str     r4, [sp, #R_VEC]
        mov     r1, #0
        str     r1, [sp, #R_ERR]
        str     r1, [sp, #R_ADDR]
        and     r6, r3, #0x1f
        cmp     r6, #MODE_USR
        add     r7, sp, #R_SP
        stmiaeq r7, {sp, lr}^
        nop
        strne   r5, [r7]
        strne   lr, [r7, #4]
        b       exc_dispatch

        .globl swi_entry
swi_entry:
        sub     sp, sp, #REGS_SIZE
        stmia   sp, {r0-r12}
        mrs     r3, spsr
        str     lr, [sp, #R_PC]
        str     r3, [sp, #R_CPSR]
        mov     r4, #VEC_SWI
        str     r4, [sp, #R_VEC]
        mov     r1, #0
        str     r1, [sp, #R_ERR]
        str     r1, [sp, #R_ADDR]
        and     r6, r3, #0x1f
        cmp     r6, #MODE_USR
        add     r7, sp, #R_SP
        add     r5, sp, #REGS_SIZE
        stmiaeq r7, {sp, lr}^
        nop
        strne   r5, [r7]
        strne   lr, [r7, #4]

exc_dispatch:
        mov     r0, sp
        bl      arm_trap
        .globl exc_return
exc_return:
        msr     cpsr_c, #(MODE_SVC | PSR_I | PSR_F)
        ldr     r1, [sp, #R_CPSR]
        msr     spsr_cxsf, r1
        and     r2, r1, #0x1f
        cmp     r2, #MODE_USR
        bne     1f
        ldr     lr, [sp, #R_PC]!
        ldmdb   sp, {r0-lr}^
        nop
        add     sp, sp, #(REGS_SIZE - R_PC)
        movs    pc, lr
1:      ldmia   sp, {r0-r12, sp, lr, pc}^
