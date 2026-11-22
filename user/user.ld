/* GOS ring 3 programs: static ELF32 at 0x40000000 (USER_BASE). Loaded by proc/exec.c */
ENTRY(_start)
PHDRS {
    text PT_LOAD FILEHDR PHDRS FLAGS(5);     /* R+X */
    data PT_LOAD FLAGS(6);                   /* R+W */
}
SECTIONS {
    . = 0x40000000 + SIZEOF_HEADERS;
    .text   : { *(.text._start) *(.text*) } :text
    .rodata : { *(.rodata*) } :text
    . = . + 0x1000;                          /* separate page: code is not writable */
    .data   : { *(.data*) } :data
    .bss    : { *(.bss*) *(COMMON) } :data
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) *(.ARM.exidx*) *(.ARM.extab*) *(.ARM.attributes) }
}
