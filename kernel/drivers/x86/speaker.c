#include "drivers/speaker.h"
#include "sys/settings.h"
#include "arch/io.h"
#include "arch/timer.h"

static volatile int on;
static volatile u32 stop_at;

void speaker_beep(u32 hz, u32 ms) {
    if (!g_set[SET_VOLUME] || hz < 40 || hz > 8000) return;
    u32 div = 1193182u / hz, f = irq_save();
    outb(0x43, 0xB6);
    outb(0x42, (u8)(div & 0xFF)); outb(0x42, (u8)(div >> 8));
    outb(0x61, (u8)(inb(0x61) | 3));
    stop_at = timer_ticks() + (ms + 9) / 10 + 1; on = 1;
    irq_restore(f);
}

void speaker_tick(void) {
    if (on && (s32)(timer_ticks() - stop_at) >= 0) { outb(0x61, (u8)(inb(0x61) & ~3)); on = 0; }
}
