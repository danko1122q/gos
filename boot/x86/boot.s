.code16
.text
.globl start
start:
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x7C00, %sp
    sti
    mov %dl, drive

    movw $0x1000, dap_seg
    movl $1, dap_lba
    mov $KSECT, %cx
.Lrd:
    push %cx
    mov $dap, %si
    mov drive, %dl
    mov $0x42, %ah
    int $0x13
    jc fail
    pop %cx
    addw $0x20, dap_seg
    incl dap_lba
    loop .Lrd

    mov $0x8000, %di
    mov $0x4F01, %ax
    mov $0x0101, %cx
    int $0x10
    cmp $0x004F, %ax
    jne fail
    mov 0x8028, %eax
    mov %eax, 0x600
    mov 0x8010, %ax
    mov %ax, 0x604

    mov $0x1130, %ax
    mov $6, %bh
    int $0x10
    push %ds
    push %es
    pop %ds
    mov %bp, %si
    mov $0x9000, %ax
    mov %ax, %es
    xor %di, %di
    mov $2048, %cx
    cld
    rep movsw
    pop %ds
    xor %ax, %ax
    mov %ax, %es

    mov $0x4F02, %ax
    mov $0x4101, %bx
    int $0x10
    cmp $0x004F, %ax
    jne fail

    movl $0, 0x610
    movl $1024, 0x614
    movl $0, 0x618
    movb $0, 0x61C
.ifndef NO_E820
    xor %ebx, %ebx
    mov $64, %si
.Le820:
    mov $0x700, %di
    mov $0xE820, %eax
    mov $0x534D4150, %edx
    mov $24, %ecx
    int $0x15
    jc .Le820end
    cmp $0x534D4150, %eax
    jne .Le820end
    cmpl $1, 0x710
    jne .Le820nx
    cmpl $0, 0x704
    jne .Le820nx
    mov 0x700, %eax
    cmpl $0, 0x70C
    jne .Lclamp
    mov 0x708, %ecx
    add %eax, %ecx
    jnc .Le820e
.Lclamp:
    mov $0x40000000, %ecx
.Le820e:
    cmp $0x40000000, %ecx
    jbe .Lc1
    mov $0x40000000, %ecx
.Lc1:
    cmp $0x100000, %eax
    jae .Lb1
    mov $0x100000, %eax
.Lb1:
    cmp %eax, %ecx
    jbe .Le820nx
    mov %ecx, %edx
    sub %eax, %edx
    cmp 0x618, %edx
    jbe .Le820nx
    mov %edx, 0x618
    shr $10, %eax
    shr $10, %ecx
    mov %eax, 0x614
    mov %ecx, 0x610
.Le820nx:
    dec %si
    jz .Le820end
    test %ebx, %ebx
    jnz .Le820
.Le820end:
    cmpl $0, 0x610
    je .Le801
    movb $1, 0x61C
    jmp .Lnoram
.Le801:
.endif
    mov $0xE801, %ax
    int $0x15
    jc .Lnoram
    test %ax, %ax
    jnz .Le8ok
    mov %cx, %ax
    mov %dx, %bx
.Le8ok:
    movzwl %ax, %eax
    movzwl %bx, %ebx
    shl $6, %ebx
    add %ebx, %eax
    add $1024, %eax
    mov %eax, 0x610
.Lnoram:

    in $0x92, %al
    or $2, %al
    out %al, $0x92
    cli
    lgdt gdt_desc
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
    ljmpl $0x08, $pm

fail:
    mov $0x0E45, %ax
    int $0x10
    hlt
    jmp .

.code32
pm:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    mov $0x9F000, %esp
    mov $0x10000, %eax
    jmp *%eax

drive:  .byte 0
dap:    .byte 0x10, 0
        .word 1
dap_off: .word 0
dap_seg: .word 0
dap_lba: .quad 0

.balign 8
gdt:    .quad 0
        .quad 0x00CF9A000000FFFF
        .quad 0x00CF92000000FFFF
gdt_desc:
        .word gdt_desc - gdt - 1
        .long gdt

.org 510
.word 0xAA55
