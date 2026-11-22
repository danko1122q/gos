#include "fs/devfs.h"
#include "drivers/serial.h"
#include "proc/task.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/errno.h"

static u32 rng = 0x2545F491;
static u32 rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static int null_read(VNode *n, u32 o, void *b, u32 l)  { (void)n; (void)o; (void)b; (void)l; return 0; }
static int null_write(VNode *n, u32 o, const void *b, u32 l) { (void)n; (void)o; (void)b; return (int)l; }
static int zero_read(VNode *n, u32 o, void *b, u32 l)  { (void)n; (void)o; memset(b, 0, l); return (int)l; }
static int rand_read(VNode *n, u32 o, void *b, u32 l)  {
    (void)n; (void)o; u8 *p = b;
    for (u32 i = 0; i < l; i++) p[i] = rnd() >> 11;
    return (int)l;
}
static int con_write(VNode *n, u32 o, const void *b, u32 l) {
    (void)n; (void)o;
    const char *s = b; char t[128];
    for (u32 i = 0; i < l; ) {
        u32 c = l - i < sizeof t - 1 ? l - i : sizeof t - 1;
        memcpy(t, s + i, c); t[c] = 0; kputs(t); i += c;
    }
    return (int)l;
}

static const VOps o_null = { null_read, null_write };
static const VOps o_zero = { zero_read, null_write };
static const VOps o_rand = { rand_read, null_write };
static int con_read(VNode *n, u32 o, void *b, u32 l) {
    (void)n; (void)o;
    char *p = b; u32 got = 0;
    if (!l) return 0;
    for (;;) {
        int c;
        while (got < l && (c = serial_try_getc()) >= 0) {
            if (c == 4) return (int)got;
            if (c == '\r') c = '\n';
            p[got++] = (char)c;
            if (c == '\n') return (int)got;
        }
        if (got) return (int)got;
        task_sleep_ms(10);
    }
}

static const VOps o_con  = { con_read, con_write };

void devfs_init(VNode *d) {
    vfs_add_dev(d, "null", &o_null, 0);
    vfs_add_dev(d, "zero", &o_zero, 0);
    vfs_add_dev(d, "random", &o_rand, 0);
    vfs_add_dev(d, "urandom", &o_rand, 0);
    vfs_add_dev(d, "console", &o_con, 0);
    vfs_add_dev(d, "tty", &o_con, 0);
}
