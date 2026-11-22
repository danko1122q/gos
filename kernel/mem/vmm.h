#ifndef MEM_VMM_H
#define MEM_VMM_H
#include "lib/types.h"
#include "mem/pmm.h"

#define PTE_P      0x001u
#define PTE_W      0x002u
#define PTE_U      0x004u
#define PTE_PS     0x080u
#define PTE_SHARED 0x200u

#define USER_BASE        0x40000000u
#define USER_TOP         0xC0000000u
#define USER_STACK_TOP   USER_TOP
#define USER_STACK_PAGES 32u
#define USER_STACK_INIT  2u
#define USER_BRK_LIMIT   0x70000000u
#define USER_SURF_BASE   0x80000000u
#define USER_SURF_STRIDE 0x00100000u
#define USER_SURF_SLOTS  4

u32  vmm_boot_frames(u32 ram, u32 heap_region_bytes);
void vmm_init(u32 ram_end, u32 fb_phys, u32 fb_size);
u32  vmm_kernel_pd(void);
u32  vmm_current(void);
void vmm_switch(u32 pd);

u32  vmm_create_space(void);
void vmm_destroy_space(u32 pd);

int  vmm_map(u32 pd, u32 va, u32 pa, u32 flags);
int  vmm_alloc_map(u32 pd, u32 va, u32 npages, u32 flags);
void vmm_unmap(u32 pd, u32 va, u32 npages, int free_frames);
u32  vmm_walk(u32 pd, u32 va);
u32  vmm_count_user_pages(u32 pd);

int  vmm_demand_fault(u32 va);

u32  vmm_pse_chunks(void);
u32  vmm_heap_base(void);
u32  vmm_heap_size(void);
#define KSURF_SLOTS   12u
#define KSURF_STRIDE  0x80000u
u32  vmm_ksurf_base(void);
int  vmm_kmap(u32 va, u32 pa);
u32  vmm_kunmap(u32 va);

#endif
