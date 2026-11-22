#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "kmi_set2.h"
#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/hw.h"

#define KMICR    0x00
#define KMISTAT  0x04
#define KMIDATA  0x08
#define KMICLK   0x0C
#define KMIIR    0x10
#define CR_EN       0x04u
#define CR_RXINTREN 0x10u
#define ST_RXFULL   0x10u
#define ST_TXEMPTY  0x40u

static KmiSet2 kst;

static int kmi_get(u32 b, u8 *v) {
    for (int t = 200000; t > 0; t--) if (rd32(b + KMISTAT) & ST_RXFULL) { *v = (u8)rd32(b + KMIDATA); return 1; }
    return 0;
}
static void kmi_put(u32 b, u8 v) {
    for (int t = 200000; t > 0; t--) if (rd32(b + KMISTAT) & ST_TXEMPTY) break;
    wr32(b + KMIDATA, v);
}
static int kmi_cmd(u32 b, u8 v) {
    for (int tries = 0; tries < 3; tries++) {
        kmi_put(b, v);
        u8 r;
        while (kmi_get(b, &r)) { if (r == 0xFA) return 1; if (r != 0xFE) continue; break; }
    }
    return 0;
}

static void kmi_up(u32 b) { wr32(b + KMICLK, 0); wr32(b + KMICR, CR_EN); while (rd32(b + KMISTAT) & ST_RXFULL) (void)rd32(b + KMIDATA); }

static void mouse_setup(void) {
    kmi_up(KMI1_BASE);
    kmi_cmd(KMI1_BASE, 0xFF);
    u8 r; while (kmi_get(KMI1_BASE, &r)) ;
    kmi_cmd(KMI1_BASE, 0xF6);
    kmi_cmd(KMI1_BASE, 0xF3); kmi_cmd(KMI1_BASE, 200);
    kmi_cmd(KMI1_BASE, 0xF3); kmi_cmd(KMI1_BASE, 100);
    kmi_cmd(KMI1_BASE, 0xF3); kmi_cmd(KMI1_BASE, 80);
    if (kmi_cmd(KMI1_BASE, 0xF2)) { u8 id; if (kmi_get(KMI1_BASE, &id) && id == 3) mouse_set_wheel(1); }
    kmi_cmd(KMI1_BASE, 0xF4);
    wr32(KMI1_BASE + KMICR, CR_EN | CR_RXINTREN);
}

static void kbd_setup(void) {
    kmi_up(KMI0_BASE);
    kmi_cmd(KMI0_BASE, 0xF4);
    wr32(KMI0_BASE + KMICR, CR_EN | CR_RXINTREN);
}

static void kbd_irq(Regs *r) {
    (void)r;
    for (int n = 0; n < 32 && (rd32(KMI0_BASE + KMISTAT) & ST_RXFULL); n++) {
        u8 o[2], d = (u8)rd32(KMI0_BASE + KMIDATA);
        int k = kmi_set2_feed(&kst, d, o);
        for (int i = 0; i < k; i++) kbd_byte(o[i]);
    }
}
static void mouse_irq(Regs *r) {
    (void)r;
    for (int n = 0; n < 32 && (rd32(KMI1_BASE + KMISTAT) & ST_RXFULL); n++) mouse_byte((u8)rd32(KMI1_BASE + KMIDATA));
}

void ps2_init(void) {
    isr_register(33, kbd_irq);
    isr_register(44, mouse_irq);
    mouse_setup();
    kbd_setup();
    pic_unmask(1);
    pic_unmask(12);
}
