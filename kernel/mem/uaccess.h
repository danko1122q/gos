#ifndef MEM_UACCESS_H
#define MEM_UACCESS_H
#include "lib/types.h"

int uaccess_ok(const void *p, u32 len, int write);
int copy_from_user(void *dst, const void *usrc, u32 n);
int copy_to_user(void *udst, const void *src, u32 n);
int strncpy_from_user(char *dst, const char *usrc, u32 max);

#endif
