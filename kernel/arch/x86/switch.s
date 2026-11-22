.code32
.text
.globl switch_context
switch_context:
    mov 4(%esp), %eax
    mov 8(%esp), %edx
    push %ebp
    push %ebx
    push %esi
    push %edi
    mov %esp, (%eax)
    mov %edx, %esp
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret

.globl enter_user
enter_user:
    mov 4(%esp), %eax
    mov 8(%esp), %ecx
    mov $0x23, %dx
    mov %dx, %ds
    mov %dx, %es
    mov %dx, %fs
    mov %dx, %gs
    pushl $0x23
    push %ecx
    pushl $0x202
    pushl $0x1B
    push %eax
    iret

.section .note.GNU-stack,"",@progbits
