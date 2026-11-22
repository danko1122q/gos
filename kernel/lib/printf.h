#ifndef LIB_PRINTF_H
#define LIB_PRINTF_H
#include <stdarg.h>
#include "lib/types.h"

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);
int kprintf(const char *fmt, ...);
void kputs(const char *s);

#endif
