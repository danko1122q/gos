#ifndef ARCH_PANIC_H
#define ARCH_PANIC_H
#include "arch/idt.h"
void panic_exception(Regs *r) __attribute__((noreturn));
void kpanic(const char *fmt, ...) __attribute__((noreturn));
#endif
