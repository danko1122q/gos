#include "arch/timer.h"
#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/hw.h"
#include "proc/task.h"
#include "drivers/keyboard.h"
#include "drivers/speaker.h"

#define T_LOAD 0x00
#define T_CTRL 0x08
#define TIMCLK 1000000u

static volatile u32 ticks;

static void irq0(Regs *r) { (void)r; ticks++; kbd_tick(); speaker_tick(); sched_tick(); }

void timer_init(void) {
    wr32(TIMER0_BASE + T_CTRL, 0);
    wr32(TIMER0_BASE + T_LOAD, TIMCLK / HZ);
    wr32(TIMER0_BASE + T_CTRL, 0x80 | 0x40 | 0x20 | 0x02);
    isr_register(32, irq0);
    pic_unmask(0);
}
u32 timer_ticks(void) { return ticks; }
u32 uptime_sec(void) { return ticks / HZ; }
u32 timer_ms_now(void) { return ticks * (1000 / HZ); }
