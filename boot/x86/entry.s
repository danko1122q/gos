.code32
.section .entry,"ax",@progbits
.globl _start
_start:
    cld
    mov $__bss_start, %edi
    mov $__bss_end, %ecx
    sub %edi, %ecx
    xor %eax, %eax
    rep stosb
    call kmain
1:  hlt
    jmp 1b

.text
.globl blit32
blit32:
    push %edi
    push %esi
    mov 12(%esp), %edi
    mov 16(%esp), %esi
    mov 20(%esp), %ecx
    cld
    rep movsl
    pop %esi
    pop %edi
    ret

.section .note.GNU-stack,"",@progbits
