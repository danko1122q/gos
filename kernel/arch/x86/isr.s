.code32
.text

.macro ISR_NOERR n
isr_stub_\n:
    pushl $0
    pushl $\n
    jmp isr_common
.endm
.macro ISR_ERR n
isr_stub_\n:
    pushl $\n
    jmp isr_common
.endm
.macro ISR_STUB n
  .if (\n == 8) || (\n >= 10 && \n <= 14) || (\n == 17) || (\n == 30)
    ISR_ERR \n
  .else
    ISR_NOERR \n
  .endif
.endm

isr_common:
    pusha
    push %ds
    push %es
    push %fs
    push %gs
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    cld
    push %esp
    call isr_dispatch
    add $4, %esp
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    add $8, %esp
    iret

.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
    ISR_STUB \n
.endr
.globl isr_stub_80
isr_stub_80:
ISR_NOERR 128

.data
.globl isr_stub_table
isr_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
    .long isr_stub_\n
.endr

.section .note.GNU-stack,"",@progbits
