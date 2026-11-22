#include "arch/arch.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "arch/pic.h"

void arch_early_init(void) {
    gdt_init();
    idt_init();
    pic_init();
}
