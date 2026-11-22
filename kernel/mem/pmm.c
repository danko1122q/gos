#include "mem/pmm.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "arch/io.h"

#define MAXFRAMES (PMM_MAX_RAM / PAGE_SIZE)

static u32 bitmap[MAXFRAMES / 32];
static u32 nframes, first_free_frame, hint, total_managed, free_count, ram_top;

static inline int  test(u32 f)  { return bitmap[f >> 5] & (1u << (f & 31)); }
static inline void setb(u32 f)  { bitmap[f >> 5] |= (1u << (f & 31)); }
static inline void clrb(u32 f)  { bitmap[f >> 5] &= ~(1u << (f & 31)); }

void pmm_init(u32 ram_base, u32 ram_end) {
    if (ram_end > PMM_MAX_RAM) ram_end = PMM_MAX_RAM;
    ram_end &= PAGE_MASK;
    if (ram_base < PMM_BASE) ram_base = PMM_BASE;
    ram_base = (ram_base + PAGE_SIZE - 1) & PAGE_MASK;
    if (ram_end < ram_base) ram_end = ram_base;
    memset(bitmap, 0xFF, sizeof bitmap);
    nframes = ram_end / PAGE_SIZE;
    first_free_frame = ram_base / PAGE_SIZE;
    for (u32 f = first_free_frame; f < nframes; f++) clrb(f);
    total_managed = free_count = nframes - first_free_frame;
    hint = first_free_frame;
    ram_top = ram_end;
}

void pmm_add_range(u32 start, u32 end) {
    u32 a = (start + PAGE_SIZE - 1) / PAGE_SIZE, b = end / PAGE_SIZE;
    if (b > first_free_frame) b = first_free_frame;
    if (a >= b) return;
    for (u32 f = a; f < b; f++) if (test(f)) { clrb(f); free_count++; total_managed++; }
    first_free_frame = a;
    hint = a;
}

u32 pmm_alloc(void) {
    u32 fl = irq_save();
    for (u32 n = 0; n < nframes - first_free_frame; n++) {
        u32 f = hint + n;
        if (f >= nframes) f = first_free_frame + (f - nframes);
        if (!test(f)) {
            setb(f); free_count--; hint = f + 1;
            irq_restore(fl);
            return f * PAGE_SIZE;
        }
    }
    irq_restore(fl);
    return 0;
}

u32 pmm_alloc_zero(void) {
    u32 p = pmm_alloc();
    if (p) memset((void *)p, 0, PAGE_SIZE);
    return p;
}

u32 pmm_alloc_contig(u32 n) {
    if (!n) return 0;
    u32 fl = irq_save();
    u32 run = 0, start = 0;
    for (u32 f = first_free_frame; f < nframes; f++) {
        if (test(f)) { run = 0; continue; }
        if (!run) start = f;
        if (++run == n) {
            for (u32 k = start; k < start + n; k++) setb(k);
            free_count -= n;
            irq_restore(fl);
            return start * PAGE_SIZE;
        }
    }
    irq_restore(fl);
    return 0;
}

u32 pmm_alloc_contig_aligned(u32 n, u32 align_frames) {
    if (!n || !align_frames || (align_frames & (align_frames - 1))) return 0;
    u32 fl = irq_save();
    u32 f = (first_free_frame + align_frames - 1) & ~(align_frames - 1);
    for (; f + n <= nframes; f += align_frames) {
        u32 k = 0;
        while (k < n && !test(f + k)) k++;
        if (k < n) continue;
        for (k = 0; k < n; k++) setb(f + k);
        free_count -= n;
        irq_restore(fl);
        return f * PAGE_SIZE;
    }
    irq_restore(fl);
    return 0;
}

void pmm_free(u32 phys) {
    u32 f = phys / PAGE_SIZE;
    if ((phys & ~PAGE_MASK) || f < first_free_frame || f >= nframes) { kprintf("pmm: invalid free %p\n", phys); return; }
    u32 fl = irq_save();
    if (!test(f)) kprintf("pmm: double free %p\n", phys);
    else { clrb(f); free_count++; if (f < hint) hint = f; }
    irq_restore(fl);
}

void pmm_free_contig(u32 phys, u32 n) { for (u32 i = 0; i < n; i++) pmm_free(phys + i * PAGE_SIZE); }

u32 pmm_total_frames(void) { return total_managed; }
u32 pmm_free_frames(void)  { return free_count; }
u32 pmm_ram_end(void)      { return ram_top; }
