#ifndef DRV_SERIAL_H
#define DRV_SERIAL_H

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
int  serial_try_getc(void);

#endif
