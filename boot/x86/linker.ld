ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 0x10000;
    .text   : { *(.entry) *(.text*) } :text
    .rodata : { *(.rodata*) } :text
    .data   : { *(.data*) } :data
    __bss_start = .;
    .bss    : { *(.bss*) *(COMMON) } :data
    __bss_end = .;
    /* Bootloader loads the image at 0x10000; the BIOS font is copied to 0x90000 and the boot stack sits below 0x9F000. */
    ASSERT(__bss_end <= 0x90000, "kernel + initrd + bss too big (512 KB max)")
}
