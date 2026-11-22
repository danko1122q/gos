#ifndef USER_MEM_HEAP_H
#define USER_MEM_HEAP_H
#include "lib/types.h"
void *kmalloc(size_t n);
void *kzalloc(size_t n);
void *krealloc(void *p, size_t n);
void  kfree(void *p);
#endif
