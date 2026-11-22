#include "drivers/ata.h"
#include "arch/hw.h"
#include "lib/printf.h"

#define POWER   0x00
#define CLOCK   0x04
#define ARG     0x08
#define CMD     0x0C
#define RESP0   0x14
#define DTIMER  0x24
#define DLEN    0x28
#define DCTRL   0x2C
#define STATUS  0x34
#define CLEAR   0x38
#define MASK0   0x3C
#define FIFO    0x80

#define ST_CRCFAIL   (1u << 0)
#define ST_DCRCFAIL  (1u << 1)
#define ST_CMDTMO    (1u << 2)
#define ST_DTMO      (1u << 3)
#define ST_TXUNDER   (1u << 4)
#define ST_RXOVER    (1u << 5)
#define ST_RESPEND   (1u << 6)
#define ST_SENT      (1u << 7)
#define ST_DATAEND   (1u << 8)
#define ST_TXFULL    (1u << 16)
#define ST_RXAVAIL   (1u << 21)

#define CMD_RESP     (1u << 6)
#define CMD_LONG     (1u << 7)
#define CMD_ENABLE   (1u << 10)

static int present, sdhc;
static u32 nsec;

static u32 rd(u32 o) { return rd32(MMCI_BASE + o); }
static void wr(u32 o, u32 v) { wr32(MMCI_BASE + o, v); }

static int sd_cmd(u32 idx, u32 arg, int resp) {
    wr(CLEAR, 0x7FF);
    wr(ARG, arg);
    u32 c = idx | CMD_ENABLE;
    if (resp) c |= CMD_RESP;
    if (resp == 2) c |= CMD_LONG;
    wr(CMD, c);
    for (int t = 0; t < 200000; t++) {
        u32 s = rd(STATUS);
        if (s & ST_CMDTMO) { wr(CLEAR, 0x7FF); return -1; }
        if (resp ? (s & ST_RESPEND) : (s & ST_SENT)) {
            if (resp == 1 && (s & ST_CRCFAIL)) { wr(CLEAR, 0x7FF); return -2; }
            wr(CLEAR, 0x7FF);
            return 0;
        }
        if ((s & ST_CRCFAIL) && resp != 3 && (s & ST_RESPEND)) { wr(CLEAR, 0x7FF); return -2; }
    }
    return -3;
}

static u32 bits(const u32 *r, int lo, int len) {
    u32 v = 0;
    for (int i = len - 1; i >= 0; i--) { int b = lo + i; v = (v << 1) | ((r[3 - b / 32] >> (b % 32)) & 1); }
    return v;
}

void ata_init(void) {
    present = 0; sdhc = 0; nsec = 0;
    wr(MASK0, 0);
    wr(POWER, 0x03);
    wr(CLOCK, 0x100 | 0x40);
    if (sd_cmd(0, 0, 0) < 0) { kprintf("sd: no card (CMD0)\n"); return; }
    int v2 = sd_cmd(8, 0x1AA, 1) == 0 && (rd(RESP0) & 0xFFF) == 0x1AA;
    u32 ocr = 0; int ok = 0;
    for (int i = 0; i < 1000; i++) {
        if (sd_cmd(55, 0, 1) < 0) break;
        if (sd_cmd(41, 0x00FF8000u | (v2 ? 0x40000000u : 0), 3) < 0) break;
        ocr = rd(RESP0);
        if (ocr & 0x80000000u) { ok = 1; break; }
    }
    if (!ok) { kprintf("sd: ACMD41 failed\n"); return; }
    sdhc = (ocr & 0x40000000u) != 0;
    if (sd_cmd(2, 0, 2) < 0) { kprintf("sd: CMD2 failed\n"); return; }
    if (sd_cmd(3, 0, 1) < 0) { kprintf("sd: CMD3 failed\n"); return; }
    u32 rca = rd(RESP0) & 0xFFFF0000u;
    if (sd_cmd(9, rca, 2) < 0) { kprintf("sd: CMD9 failed\n"); return; }
    u32 csd[4] = { rd(RESP0), rd(RESP0 + 4), rd(RESP0 + 8), rd(RESP0 + 12) };
    if (bits(csd, 126, 2) == 1) nsec = (bits(csd, 48, 22) + 1) * 1024u;
    else {
        u32 bl = bits(csd, 80, 4), cs = bits(csd, 62, 12), mult = bits(csd, 47, 3);
        nsec = ((cs + 1) << (mult + 2)) * (1u << bl) / 512u;
    }
    if (sd_cmd(7, rca, 1) < 0) { kprintf("sd: CMD7 failed\n"); return; }
    if (!sdhc && sd_cmd(16, 512, 1) < 0) { kprintf("sd: CMD16 failed\n"); return; }
    present = 1;
    kprintf("sd: card %s, %u sectors (%u KB)\n", sdhc ? "SDHC" : "SDSC", nsec, nsec / 2);
}

int ata_present(void) { return present; }
u32 ata_sectors(void) { return nsec; }

static int xfer(u32 lba, void *buf, int write) {
    u32 *w = buf;
    wr(CLEAR, 0x7FF);
    wr(DTIMER, 0xFFFFFFF);
    wr(DLEN, 512);
    wr(DCTRL, 1u | (write ? 0u : 2u) | (9u << 4));
    if (sd_cmd(write ? 24 : 17, sdhc ? lba : lba * 512u, 1) < 0) { wr(DCTRL, 0); return -1; }
    int n = 0;
    for (int t = 0; t < 2000000 && n < 128; t++) {
        u32 s = rd(STATUS);
        if (s & (ST_DCRCFAIL | ST_DTMO | ST_TXUNDER | ST_RXOVER)) { wr(DCTRL, 0); wr(CLEAR, 0x7FF); return -1; }
        if (write) { while (n < 128 && !(rd(STATUS) & ST_TXFULL)) wr(FIFO, w[n++]); }
        else       { while (n < 128 && (rd(STATUS) & ST_RXAVAIL)) w[n++] = rd(FIFO); }
    }
    if (n < 128) { wr(DCTRL, 0); return -1; }
    for (int t = 0; t < 2000000; t++) if (rd(STATUS) & ST_DATAEND) break;
    wr(CLEAR, 0x7FF);
    wr(DCTRL, 0);
    return 0;
}

int ata_read(u32 lba, u32 count, void *buf) {
    if (!present) return -1;
    for (u32 i = 0; i < count; i++) if (xfer(lba + i, (u8 *)buf + i * 512, 0)) return -1;
    return 0;
}
int ata_write(u32 lba, u32 count, const void *buf) {
    if (!present) return -1;
    for (u32 i = 0; i < count; i++) if (xfer(lba + i, (void *)((const u8 *)buf + i * 512), 1)) return -1;
    return 0;
}
