        .syntax unified
        .arm
        .text

        .globl switch_context
switch_context:
        push    {r4-r11, lr}
        str     sp, [r0]
        mov     sp, r1
        pop     {r4-r11, pc}

        .globl enter_user
enter_user:
        mov     sp, r2
        mov     r4, r0
        mov     r5, #0
        push    {r1, r5}
        mov     r3, sp
        ldmia   r3, {sp, lr}^
        nop
        add     sp, sp, #8
        mov     r0, #0x50
        msr     spsr_cxsf, r0
        mov     lr, r4
        mov     r0, #0
        mov     r1, #0
        mov     r2, #0
        mov     r3, #0
        mov     r4, #0
        mov     r5, #0
        mov     r6, #0
        mov     r7, #0
        mov     r8, #0
        mov     r9, #0
        mov     r10, #0
        mov     r11, #0
        mov     r12, #0
        movs    pc, lr
