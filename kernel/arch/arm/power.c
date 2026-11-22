#include "arch/power.h"
#include "arch/io.h"
#include "arch/hw.h"

extern void arm_soft_reset(u32 atags) __attribute__((noreturn));
extern u32 arm_atags;

void sys_reboot(void) {
    cli();
    arm_soft_reset(arm_atags);
}

void sys_poweroff(void) {
    cli();
    wr32(SYSCTL_BASE + 0x20, 0xA05F);
    wr32(SYSCTL_BASE + 0x40, 0x105);
    for (;;) hlt();
}
