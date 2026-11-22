#include "drivers/ata.h"
#include "arch/io.h"
#include "proc/task.h"
#include "lib/klog.h"

#define IO   0x1F0
#define CTRL 0x3F6
#define ST_ERR 0x01
#define ST_DRQ 0x08
#define ST_DF  0x20
#define ST_BSY 0x80

static int present; static u32 nsect;

static u8 status(void) { return inb(IO + 7); }
static void delay400(void) { for (int i = 0; i < 4; i++) inb(CTRL); }

static int wait_bsy(void) {
    for (int t = 0; t < 2000000; t++) if (!(status() & ST_BSY)) return 0;
    return -1;
}
static int wait_drq(void) {
    for (int t = 0; t < 2000000; t++) {
        u8 s = status();
        if (s & (ST_ERR | ST_DF)) return -1;
        if (!(s & ST_BSY) && (s & ST_DRQ)) return 0;
    }
    return -1;
}

void ata_init(void) {
    outb(CTRL, 0x02);
    outb(IO + 6, 0xA0); delay400();
    if (status() == 0xFF) return;
    outb(IO + 2, 0); outb(IO + 3, 0); outb(IO + 4, 0); outb(IO + 5, 0);
    outb(IO + 7, 0xEC);
    if (!status()) return;
    if (wait_bsy()) return;
    if (inb(IO + 4) || inb(IO + 5)) return;
    if (wait_drq()) return;
    u16 id[256];
    insw(IO, id, 256);
    nsect = id[60] | ((u32)id[61] << 16);
    present = nsect > 0;
    if (present) klog("ata: disk %u sectors (%u MB)\n", nsect, nsect / 2048);
}

int  ata_present(void) { return present; }
u32  ata_sectors(void) { return nsect; }

static int setup(u32 lba, u32 n, u8 cmd) {
    if (wait_bsy()) return -1;
    outb(IO + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(IO + 2, (u8)n);
    outb(IO + 3, lba & 0xFF); outb(IO + 4, (lba >> 8) & 0xFF); outb(IO + 5, (lba >> 16) & 0xFF);
    outb(IO + 7, cmd);
    return 0;
}

int ata_read(u32 lba, u32 count, void *buf) {
    if (!present || !count || lba + count > nsect) return -1;
    u8 *p = buf; int r = 0;
    preempt_disable();
    while (count && !r) {
        u32 n = count > 128 ? 128 : count;
        if (setup(lba, n, 0x20)) { r = -1; break; }
        for (u32 i = 0; i < n; i++) {
            delay400();
            if (wait_drq()) { r = -1; break; }
            insw(IO, p, 256); p += 512;
        }
        lba += n; count -= n;
    }
    preempt_enable();
    return r;
}

int ata_write(u32 lba, u32 count, const void *buf) {
    if (!present || !count || lba + count > nsect) return -1;
    const u8 *p = buf; int r = 0;
    preempt_disable();
    while (count && !r) {
        u32 n = count > 128 ? 128 : count;
        if (setup(lba, n, 0x30)) { r = -1; break; }
        for (u32 i = 0; i < n; i++) {
            delay400();
            if (wait_drq()) { r = -1; break; }
            outsw(IO, p, 256); p += 512;
        }
        lba += n; count -= n;
    }
    if (!r) { outb(IO + 7, 0xE7); wait_bsy(); }
    preempt_enable();
    return r;
}
