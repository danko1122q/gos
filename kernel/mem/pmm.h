#ifndef MEM_PMM_H
#define MEM_PMM_H
#include "lib/types.h"
#include "arch/arch.h"

#define PAGE_SIZE   4096u
#define PAGE_MASK   0xFFFFF000u

#define PMM_BASE    0x100000u

void pmm_add_range(u32 start, u32 end);
void pmm_init(u32 ram_base, u32 ram_end);
u32  pmm_alloc(void);
u32  pmm_alloc_zero(void);
u32  pmm_alloc_contig(u32 n);
u32  pmm_alloc_contig_aligned(u32 n, u32 align_frames);
void pmm_free(u32 phys);
void pmm_free_contig(u32 phys, u32 n);
u32  pmm_total_frames(void);
u32  pmm_free_frames(void);
u32  pmm_ram_end(void);

#endif
