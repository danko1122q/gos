#include "arch/arch.h"
#include "arch/pic.h"
#include "arch/hw.h"
#include "lib/printf.h"

void arch_early_init(void) {
    pic_init();
}

int arch_cpuinfo(char *b, int m) {
    u32 id; __asm__ volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(id));
    return ksnprintf(b, m, "vendor_id : ARM\nmain_id   : 0x%08x\narch      : %s\n", id, GOS_ARCH_DESC);
}
