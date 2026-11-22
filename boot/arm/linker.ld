/* ARM32 kernel: QEMU loads it (-kernel build/kernel.bin) at 0x10000. Physical memory layout is the same as x86:
 * 0x10000..0x90000 kernel+initrd+bss (512 KB max), 0x90000..0x9F000 boot stack (no BIOS font on ARM), free RAM starts at 1 MB. */
OUTPUT_FORMAT("elf32-littlearm")
ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 0x10000;
    .text   : { *(.entry) *(.text*) } :text
    .rodata : { *(.rodata*) } :text
    . = ALIGN(4096);
    .vectors : { *(.vectors) } :text          /* one page: mapped again at 0xFFFF0000 (high vectors) */
    .data   : { *(.data*) } :data
    __bss_start = .;
    .bss    : { *(.bss*) *(COMMON) . = ALIGN(4); } :data
    __bss_end = .;
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) *(.ARM.attributes) }
    ASSERT(__bss_end <= 0x90000, "kernel + initrd + bss too big (512 KB max)")
}
