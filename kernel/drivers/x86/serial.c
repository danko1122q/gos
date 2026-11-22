#include "drivers/serial.h"
#include "arch/io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x03);
}
void serial_putc(char c) {
    int t = 100000;
    while (!(inb(COM1 + 5) & 0x20) && t--) ;
    outb(COM1, (u8)c);
}
void serial_puts(const char *s) {
    for (; *s; s++) { if (*s == '\n') serial_putc('\r'); serial_putc(*s); }
}
int serial_try_getc(void) { return (inb(COM1 + 5) & 1) ? (int)inb(COM1) : -1; }
