#ifndef LIB_BUF_H
#define LIB_BUF_H
#include "lib/types.h"
typedef struct Buf { char *p; int len, cap; } Buf;

void buf_init(Buf *b);
void buf_free(Buf *b);
int  buf_put(Buf *b, const void *d, int n);
int  buf_putc(Buf *b, char c);
int  buf_puts(Buf *b, const char *s);
int  buf_printf(Buf *b, const char *fmt, ...);
#endif
