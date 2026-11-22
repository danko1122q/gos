        .arm
        .section .text._start,"ax",%progbits
        .globl  _start
_start:
        mov     fp, #0
        mov     r0, sp
        bic     sp, sp, #7
        bl      __libc_start
1:      mov     r7, #1
        mov     r0, #0
        svc     #0
        b       1b
        .section .note.GNU-stack,"",%progbits
