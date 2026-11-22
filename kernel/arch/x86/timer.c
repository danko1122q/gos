#include "arch/timer.h"
#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/io.h"
#include "proc/task.h"
#include "drivers/keyboard.h"
#include "drivers/speaker.h"

static volatile u32 ticks;

static void irq0(Regs *r) { (void)r; ticks++; kbd_tick(); speaker_tick(); sched_tick(); }

void timer_init(void) {
    u32 div = 1193182 / HZ;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF); outb(0x40, div >> 8);
    isr_register(32, irq0);
    pic_unmask(0);
}
u32 timer_ticks(void) { return ticks; }
u32 uptime_sec(void) { return ticks / HZ; }
u32 timer_ms_now(void) { return ticks * (1000 / HZ); }
