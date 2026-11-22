#include "arch/pic.h"
#include "arch/io.h"

#define M_CMD 0x20
#define M_DAT 0x21
#define S_CMD 0xA0
#define S_DAT 0xA1

static void wait(void) { outb(0x80, 0); }

void pic_init(void) {
    outb(M_CMD, 0x11); wait(); outb(S_CMD, 0x11); wait();
    outb(M_DAT, 32);   wait(); outb(S_DAT, 40);   wait();
    outb(M_DAT, 4);    wait(); outb(S_DAT, 2);    wait();
    outb(M_DAT, 1);    wait(); outb(S_DAT, 1);    wait();
    outb(M_DAT, 0xFB); outb(S_DAT, 0xFF);
}
void pic_eoi(int irq) { if (irq >= 8) outb(S_CMD, 0x20); outb(M_CMD, 0x20); }
void pic_mask(int irq) {
    u16 p = irq < 8 ? M_DAT : S_DAT;
    outb(p, inb(p) | (1 << (irq & 7)));
}
void pic_unmask(int irq) {
    u16 p = irq < 8 ? M_DAT : S_DAT;
    outb(p, inb(p) & ~(1 << (irq & 7)));
}
