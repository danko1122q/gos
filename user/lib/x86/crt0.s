.code32
.section .text._start,"ax",@progbits
.globl _start
_start:
    xor %ebp, %ebp
    mov %esp, %eax
    and $-16, %esp
    push %eax
    call __libc_start
1:  mov $1, %eax
    xor %ebx, %ebx
    int $0x80
    jmp 1b

.section .note.GNU-stack,"",@progbits
