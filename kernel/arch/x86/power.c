#include "arch/power.h"
#include "arch/io.h"

void sys_reboot(void) {
    cli();
    for (int i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) break;
    outb(0x64, 0xFE);
    for (;;) hlt();
}
void sys_poweroff(void) {
    cli();
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    for (;;) hlt();
}
