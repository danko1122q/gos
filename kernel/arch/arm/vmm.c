#include "mem/vmm.h"
#include "arch/idt.h"
#include "arch/panic.h"
#include "arch/io.h"
#include "arch/hw.h"
#include "proc/task.h"
#include "proc/exec.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/errno.h"

#define L1_ENTRIES   4096u
#define L1_FRAMES    VMM_PD_FRAMES
#define VEC_VA       0xFFFF0000u

#define D_COARSE     0x1u
#define D_SECTION    0x2u
#define D_PAGE       0x2u
#define SEC_KRW      (0x1u << 10)
#define PG_AP(ap)    ((ap) * 0x550u)

static u32 kpd, heap_base, heap_size, region_size, sec_chunks;
extern char arm_vectors[];

static inline int is_user_va(u32 va) { return va >= USER_BASE && va < USER_TOP; }
static inline int is_surf_va(u32 va) { return va >= USER_SURF_BASE && va < USER_SURF_BASE + USER_SURF_SLOTS * USER_SURF_STRIDE; }

static inline void tlb_flush_all(void) { __asm__ volatile("mcr p15, 0, %0, c8, c7, 0" :: "r"(0) : "memory"); }
static inline void tlb_flush_va(u32 va) { __asm__ volatile("mcr p15, 0, %0, c8, c7, 1" :: "r"(va) : "memory"); }

u32 vmm_current(void) { u32 c; __asm__ volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(c)); return c & ~0x3FFFu; }
void vmm_switch(u32 pd) {
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 0" :: "r"(pd) : "memory");
    tlb_flush_all();
}
u32 vmm_kernel_pd(void) { return kpd; }

static u32 pte_flags(u32 flags) {
    u32 ap = (flags & PTE_U) ? ((flags & PTE_W) ? 3u : 2u) : 1u;
    return PG_AP(ap) | D_PAGE;
}

static u32 *get_l2(u32 pd, u32 va, int create) {
    u32 *l1 = (u32 *)pd;
    u32 i = va >> 20, e = l1[i];
    if ((e & 3) == D_SECTION) return NULL;
    if ((e & 3) == D_COARSE) return (u32 *)(e & ~0x3FFu);
    if (!create) return NULL;
    u32 g = pmm_alloc_zero();
    if (!g) return NULL;
    u32 b = i & ~3u;
    for (u32 k = 0; k < 4; k++) l1[b + k] = (g + k * 1024u) | D_COARSE;
    return (u32 *)(l1[i] & ~0x3FFu);
}

int vmm_map(u32 pd, u32 va, u32 pa, u32 flags) {
    if ((flags & PTE_SHARED) != 0 && !is_surf_va(va)) { kprintf("vmm: PTE_SHARED outside the surface region (%p)\n", va); return -EINVAL; }
    u32 *t = get_l2(pd, va, 1);
    if (!t) return -ENOMEM;
    t[(va >> 12) & 0xFF] = (pa & PAGE_MASK) | pte_flags(flags);
    if (pd == vmm_current()) tlb_flush_va(va);
    return 0;
}

u32 vmm_walk(u32 pd, u32 va) {
    u32 e = ((u32 *)pd)[va >> 20];
    u32 pa, fl, ap;
    if ((e & 3) == D_SECTION) {
        pa = (e & 0xFFF00000u) | (va & 0x000FF000u);
        ap = (e >> 10) & 3;
    } else if ((e & 3) == D_COARSE) {
        u32 p = ((u32 *)(e & ~0x3FFu))[(va >> 12) & 0xFF];
        if ((p & 3) != D_PAGE) return 0;
        pa = p & PAGE_MASK;
        ap = (p >> 4) & 3;
    } else return 0;
    fl = PTE_P;
    if (ap == 3) fl |= PTE_U | PTE_W;
    else if (ap == 2) fl |= PTE_U;
    else fl |= PTE_W;
    if (is_surf_va(va)) fl |= PTE_SHARED;
    return pa | fl;
}

void vmm_unmap(u32 pd, u32 va, u32 npages, int free_frames) {
    int cur = pd == vmm_current();
    for (u32 i = 0; i < npages; i++, va += PAGE_SIZE) {
        u32 *t = get_l2(pd, va, 0);
        if (!t) continue;
        u32 p = t[(va >> 12) & 0xFF];
        if ((p & 3) != D_PAGE) continue;
        if (free_frames && !is_surf_va(va)) pmm_free(p & PAGE_MASK);
        t[(va >> 12) & 0xFF] = 0;
        if (cur) tlb_flush_va(va);
    }
}

int vmm_alloc_map(u32 pd, u32 va, u32 npages, u32 flags) {
    for (u32 i = 0; i < npages; i++) {
        u32 f = pmm_alloc_zero();
        if (!f || vmm_map(pd, va + i * PAGE_SIZE, f, flags) < 0) {
            if (f) pmm_free(f);
            vmm_unmap(pd, va, i, 1);
            return -ENOMEM;
        }
    }
    return 0;
}

u32 vmm_create_space(void) {
    u32 pd = pmm_alloc_contig_aligned(L1_FRAMES, L1_FRAMES);
    if (!pd) return 0;
    u32 *d = (u32 *)pd, *k = (u32 *)kpd;
    memset(d, 0, L1_FRAMES * PAGE_SIZE);
    for (u32 i = 0; i < L1_ENTRIES; i++)
        if (i < (USER_BASE >> 20) || i >= (USER_TOP >> 20)) d[i] = k[i];
    return pd;
}

static u32 group_pages(const u32 *t, int release) {
    u32 n = 0;
    for (u32 j = 0; j < 1024; j++) {
        if ((t[j] & 3) != D_PAGE) continue;
        n++;
        if (release) pmm_free(t[j] & PAGE_MASK);
    }
    return n;
}

void vmm_destroy_space(u32 pd) {
    u32 *d = (u32 *)pd;
    for (u32 i = USER_BASE >> 20; i < (USER_TOP >> 20); i += 4) {
        if ((d[i] & 3) != D_COARSE) continue;
        u32 *g = (u32 *)(d[i] & PAGE_MASK);
        for (u32 k = 0; k < 4; k++) {
            u32 va0 = (i + k) << 20;
            if (is_surf_va(va0)) continue;
            const u32 *t = g + k * 256;
            for (u32 j = 0; j < 256; j++) if ((t[j] & 3) == D_PAGE) pmm_free(t[j] & PAGE_MASK);
        }
        pmm_free((u32)g);
    }
    for (u32 f = 0; f < L1_FRAMES; f++) pmm_free(pd + f * PAGE_SIZE);
}

u32 vmm_ksurf_base(void) { return heap_base + heap_size; }
u32 vmm_pse_chunks(void) { return sec_chunks; }
u32 vmm_heap_base(void) { return heap_base; }
u32 vmm_heap_size(void) { return heap_size; }

int vmm_kmap(u32 va, u32 pa) {
    if (va < heap_base || va >= heap_base + region_size) return -EINVAL;
    u32 *t = get_l2(kpd, va, 0);
    if (!t) return -ENOMEM;
    t[(va >> 12) & 0xFF] = (pa & PAGE_MASK) | PG_AP(1) | D_PAGE;
    tlb_flush_va(va);
    return 0;
}

u32 vmm_kunmap(u32 va) {
    if (va < heap_base || va >= heap_base + region_size) return 0;
    u32 *t = get_l2(kpd, va, 0);
    if (!t) return 0;
    u32 p = t[(va >> 12) & 0xFF];
    t[(va >> 12) & 0xFF] = 0;
    tlb_flush_va(va);
    return (p & 3) == D_PAGE ? p & PAGE_MASK : 0;
}

u32 vmm_count_user_pages(u32 pd) {
    u32 n = 0, *d = (u32 *)pd;
    for (u32 i = USER_BASE >> 20; i < (USER_TOP >> 20); i += 4) {
        if ((d[i] & 3) != D_COARSE) continue;
        const u32 *g = (const u32 *)(d[i] & PAGE_MASK);
        for (u32 k = 0; k < 4; k++)
            if (!is_surf_va((i + k) << 20)) n += group_pages(g + k * 256, 0);
        n++;
    }
    return n + L1_FRAMES;
}

int vmm_demand_fault(u32 va) {
    Task *t = task_current();
    if (!t || !t->pd || t->pd != vmm_current()) return 0;
    va &= PAGE_MASK;
    if (!is_user_va(va)) return 0;
    int in_stack = va >= USER_STACK_TOP - USER_STACK_PAGES * PAGE_SIZE && va < USER_STACK_TOP;
    int in_lazy  = t->lazy_lo && va >= t->lazy_lo && va < ((t->brk + PAGE_SIZE - 1) & PAGE_MASK);
    if (!in_stack && !in_lazy) return 0;
    if (vmm_walk(t->pd, va) & PTE_P) return 0;
    u32 f = pmm_alloc_zero();
    if (!f) return 0;
    if (vmm_map(t->pd, va, f, PTE_U | PTE_W) < 0) { pmm_free(f); return 0; }
    return 1;
}

static void pf_handler(Regs *r) {
    if (REGS_USER(r)) {
        if (!(r->err & 1) && vmm_demand_fault(r->addr)) return;
        proc_fault(r, r->addr);
        return;
    }
    panic_exception(r);
}

u32 vmm_boot_frames(u32 ram, u32 hsz) {
    (void)ram;
    return L1_FRAMES + 1 + 1 + (hsz + KSURF_SLOTS * KSURF_STRIDE + 0x3FFFFFu) / 0x400000u;
}

void vmm_init(u32 ram_end, u32 fb_phys, u32 fb_size) {
    (void)fb_phys; (void)fb_size;
    u32 ram_total = ram_end;
    if (ram_end > PMM_MAX_RAM) ram_end = PMM_MAX_RAM;
    kpd = pmm_alloc_contig_aligned(L1_FRAMES, L1_FRAMES);
    if (!kpd) kpanic("vmm: no aligned 16 KB for the L1 table");
    memset((void *)kpd, 0, L1_FRAMES * PAGE_SIZE);
    u32 *l1 = (u32 *)kpd;

    u32 t0 = pmm_alloc_zero();
    if (!t0) kpanic("vmm: out of frames for page table 0");
    for (u32 a = PAGE_SIZE; a < 0x100000u; a += PAGE_SIZE) ((u32 *)t0)[a >> 12] = a | PG_AP(1) | D_PAGE;
    l1[0] = t0 | D_COARSE;
    for (u32 c = 0x100000u; c < ram_end; c += 0x100000u) { l1[c >> 20] = c | SEC_KRW | D_SECTION; sec_chunks++; }
    for (u32 c = PERI_BASE; c < PERI_END; c += 0x100000u) l1[c >> 20] = c | SEC_KRW | D_SECTION;
    {
        u32 *t = get_l2(kpd, VEC_VA, 1);
        if (!t) kpanic("vmm: out of frames for the vector page");
        t[(VEC_VA >> 12) & 0xFF] = (u32)arm_vectors | PG_AP(1) | D_PAGE;
    }
    {
        u32 want = ((ram_total / 2) + 0x3FFFFFu) & ~0x3FFFFFu;
        if (want < 0x400000u) want = 0x400000u;
        if (want > 0x4000000u) want = 0x4000000u;
        u32 total = want + KSURF_SLOTS * KSURF_STRIDE;
        total = (total + 0x3FFFFFu) & ~0x3FFFFFu;
        u32 base = 0xD0000000u;
        if (base + total > VEC_VA || base + total < base) kpanic("vmm: heap region collides with the vector page");
        heap_base = base; heap_size = want; region_size = total;
        for (u32 va = base; va < base + total; va += 0x400000u)
            if (!get_l2(kpd, va, 1)) kpanic("vmm: out of frames for heap/surface L2 tables");
    }
    isr_register(14, pf_handler);
    isr_register(17, pf_handler);

    u32 sc;
    __asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(1));
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 0" :: "r"(kpd));
    tlb_flush_all();
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sc));
    sc |= 0x1u | 0x2000u;
    sc &= ~(0x2u | 0x4u | 0x8u | 0x1000u);
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0\n\tnop\n\tnop" :: "r"(sc) : "memory");
    kprintf("vmm: MMU on (ARM short-descriptor), identity %u MB (%u 1 MB sections), vectors at %p, L1 %p\n", ram_end >> 20, sec_chunks, VEC_VA, kpd);
}
