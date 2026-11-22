#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/io.h"

static void kw(void) { int t = 100000; while ((inb(0x64) & 2) && t--) ; }
static void kr(void) { int t = 100000; while (!(inb(0x64) & 1) && t--) ; }
static void mwrite(u8 v) { kw(); outb(0x64, 0xD4); kw(); outb(0x60, v); kr(); inb(0x60); }

static int mread(u8 *v) {
    for (int t = 100000; t > 0; t--) if (inb(0x64) & 1) { *v = inb(0x60); return 1; }
    return 0;
}

static void mouse_probe_wheel(void) {
    mwrite(0xF3); mwrite(200);
    mwrite(0xF3); mwrite(100);
    mwrite(0xF3); mwrite(80);
    mwrite(0xF2);
    u8 id = 0xFF;
    if (mread(&id) && id == 3) mouse_set_wheel(1);
}

static void mouse_init(void) {
    kw(); outb(0x64, 0xA8);
    kw(); outb(0x64, 0x20); kr(); u8 st = inb(0x60);
    st |= 2; st &= ~0x20;
    kw(); outb(0x64, 0x60); kw(); outb(0x60, st);
    mwrite(0xF6);
    mouse_probe_wheel();
    mwrite(0xF4);
}

static void ps2_irq(Regs *r) {
    (void)r;
    for (int n = 0; n < 32; n++) {
        u8 s = inb(0x64);
        if (!(s & 1)) break;
        u8 d = inb(0x60);
        if (s & 0x20) mouse_byte(d); else kbd_byte(d);
    }
}

void ps2_init(void) {
    isr_register(33, ps2_irq);
    isr_register(44, ps2_irq);
    mouse_init();
    while (inb(0x64) & 1) inb(0x60);
    pic_unmask(1);
    pic_unmask(12);
}
