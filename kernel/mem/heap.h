#ifndef MEM_HEAP_H
#define MEM_HEAP_H
#include "lib/types.h"

void   heap_init(void);
void  *kmalloc(size_t n);
void  *kzalloc(size_t n);
void  *krealloc(void *p, size_t n);
void   kfree(void *p);
u32    heap_used(void);
u32    heap_total(void);
u32    heap_peak(void);
u32    mem_total_kb(void);
const char *mem_map_source(void);
u32    mem_base_kb(void);

#endif
