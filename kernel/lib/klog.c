#include "lib/klog.h"
#include "lib/printf.h"
#include "arch/io.h"
#include "drivers/serial.h"

#define KLOG_SZ KLOG_SIZE
static char ring[KLOG_SZ];
static u32 total;

void klog_write(const char *s) {
    u32 f = irq_save();
    for (; *s; s++) ring[total++ % KLOG_SZ] = *s;
    irq_restore(f);
}
int klog_read(char *buf, int max) {
    u32 f = irq_save();
    u32 n = total < KLOG_SZ ? total : KLOG_SZ;
    if ((u32)max < n) n = max;
    u32 start = total - n;
    for (u32 i = 0; i < n; i++) buf[i] = ring[(start + i) % KLOG_SZ];
    irq_restore(f);
    return (int)n;
}
int klog(const char *fmt, ...) {
    char b[256];
    va_list ap; va_start(ap, fmt);
    int n = kvsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    klog_write(b); serial_puts(b);
    return n;
}

int kprintf(const char *fmt, ...) {
    char b[256];
    va_list ap; va_start(ap, fmt);
    int n = kvsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    serial_puts(b); klog_write(b);
    return n;
}
void kputs(const char *s) { serial_puts(s); klog_write(s); }
