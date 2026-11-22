#include "drivers/serial.h"
#include "arch/hw.h"

#define UARTDR   0x00
#define UARTFR   0x18
#define UARTIBRD 0x24
#define UARTFBRD 0x28
#define UARTLCRH 0x2C
#define UARTCR   0x30
#define UARTIMSC 0x38
#define FR_RXFE  0x10u
#define FR_TXFF  0x20u

void serial_init(void) {
    wr32(UART0_BASE + UARTCR, 0);
    wr32(UART0_BASE + UARTIBRD, 13);
    wr32(UART0_BASE + UARTFBRD, 1);
    wr32(UART0_BASE + UARTLCRH, 0x70);
    wr32(UART0_BASE + UARTIMSC, 0);
    wr32(UART0_BASE + UARTCR, 0x301);
}
void serial_putc(char c) {
    int t = 100000;
    while ((rd32(UART0_BASE + UARTFR) & FR_TXFF) && t--) ;
    wr32(UART0_BASE + UARTDR, (u8)c);
}
void serial_puts(const char *s) {
    for (; *s; s++) { if (*s == '\n') serial_putc('\r'); serial_putc(*s); }
}
int serial_try_getc(void) { return (rd32(UART0_BASE + UARTFR) & FR_RXFE) ? -1 : (int)(rd32(UART0_BASE + UARTDR) & 0xFF); }
