#include "mem/vmm.h"
#include "arch/idt.h"
#include "arch/panic.h"
#include "arch/io.h"
#include "proc/task.h"
#include "proc/exec.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/errno.h"

static u32 kpd, heap_base, heap_size, region_size, pse_chunks;

static inline int is_user_va(u32 va) { return va >= USER_BASE && va < USER_TOP; }
static inline void invlpg(u32 va) { __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory"); }

u32 vmm_current(void) { u32 c; __asm__ volatile("mov %%cr3, %0" : "=r"(c)); return c; }
void vmm_switch(u32 pd) { __asm__ volatile("mov %0, %%cr3" :: "r"(pd) : "memory"); }
u32  vmm_kernel_pd(void) { return kpd; }

static u32 *get_pt(u32 pd, u32 va, int create) {
    u32 *dir = (u32 *)pd;
    u32 e = dir[va >> 22];
    if (e & PTE_PS) return NULL;
    if (!(e & PTE_P)) {
        if (!create) return NULL;
        u32 pt = pmm_alloc_zero();
        if (!pt) return NULL;
        dir[va >> 22] = pt | PTE_P | PTE_W | (is_user_va(va) ? PTE_U : 0);
        return (u32 *)pt;
    }
    return (u32 *)(e & PTE_P ? (e & PAGE_MASK) : 0);
}

int vmm_map(u32 pd, u32 va, u32 pa, u32 flags) {
    u32 *pt = get_pt(pd, va, 1);
    if (!pt) return -ENOMEM;
    pt[(va >> 12) & 1023] = (pa & PAGE_MASK) | flags | PTE_P;
    if (pd == vmm_current()) invlpg(va);
    return 0;
}

u32 vmm_walk(u32 pd, u32 va) {
    u32 *pt = get_pt(pd, va, 0);
    return pt ? pt[(va >> 12) & 1023] : 0;
}

void vmm_unmap(u32 pd, u32 va, u32 npages, int free_frames) {
    int cur = pd == vmm_current();
    for (u32 i = 0; i < npages; i++, va += PAGE_SIZE) {
        u32 *pt = get_pt(pd, va, 0);
        if (!pt) continue;
        u32 e = pt[(va >> 12) & 1023];
        if (!(e & PTE_P)) continue;
        if (free_frames && !(e & PTE_SHARED)) pmm_free(e & PAGE_MASK);
        pt[(va >> 12) & 1023] = 0;
        if (cur) invlpg(va);
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
    u32 pd = pmm_alloc_zero();
    if (!pd) return 0;
    u32 *d = (u32 *)pd, *k = (u32 *)kpd;
    for (int i = 0; i < 1024; i++)
        if (i < (int)(USER_BASE >> 22) || i >= (int)(USER_TOP >> 22)) d[i] = k[i];
    return pd;
}

void vmm_destroy_space(u32 pd) {
    u32 *d = (u32 *)pd;
    for (u32 i = USER_BASE >> 22; i < (USER_TOP >> 22); i++) {
        if (!(d[i] & PTE_P)) continue;
        u32 *pt = (u32 *)(d[i] & PAGE_MASK);
        for (int j = 0; j < 1024; j++)
            if ((pt[j] & PTE_P) && !(pt[j] & PTE_SHARED)) pmm_free(pt[j] & PAGE_MASK);
        pmm_free((u32)pt);
    }
    pmm_free(pd);
}

u32 vmm_ksurf_base(void) { return heap_base + heap_size; }
u32 vmm_pse_chunks(void) { return pse_chunks; }
u32 vmm_heap_base(void) { return heap_base; }
u32 vmm_heap_size(void) { return heap_size; }

int vmm_kmap(u32 va, u32 pa) {
    if (va < heap_base || va >= heap_base + region_size) return -EINVAL;
    u32 *pt = get_pt(kpd, va, 0);
    if (!pt) return -ENOMEM;
    pt[(va >> 12) & 1023] = (pa & PAGE_MASK) | PTE_W | PTE_P;
    invlpg(va);
    return 0;
}

u32 vmm_kunmap(u32 va) {
    if (va < heap_base || va >= heap_base + region_size) return 0;
    u32 *pt = get_pt(kpd, va, 0);
    if (!pt) return 0;
    u32 e = pt[(va >> 12) & 1023];
    pt[(va >> 12) & 1023] = 0;
    invlpg(va);
    return e & PTE_P ? e & PAGE_MASK : 0;
}

u32 vmm_count_user_pages(u32 pd) {
    u32 n = 0, *d = (u32 *)pd;
    for (u32 i = USER_BASE >> 22; i < (USER_TOP >> 22); i++) {
        if (!(d[i] & PTE_P)) continue;
        u32 *pt = (u32 *)(d[i] & PAGE_MASK);
        for (int j = 0; j < 1024; j++) if ((pt[j] & PTE_P) && !(pt[j] & PTE_SHARED)) n++;
        n++;
    }
    return n + 1;
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
    u32 cr2; __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    if (r->cs & 3) {
        if (!(r->err & 1) && vmm_demand_fault(cr2)) return;
        proc_fault(r, cr2); return;
    }
    panic_exception(r);
}

u32 vmm_boot_frames(u32 ram, u32 hsz) {
    return 1 + ((ram > USER_BASE ? USER_BASE : ram) + 0x3FFFFFu) / 0x400000u + (hsz + KSURF_SLOTS * KSURF_STRIDE + 0x3FFFFFu) / 0x400000u;
}

void vmm_init(u32 ram_end, u32 fb_phys, u32 fb_size) {
    u32 ram_total = ram_end;
    if (ram_end > USER_BASE) ram_end = USER_BASE;
    kpd = pmm_alloc_zero();
    if (!kpd) kpanic("vmm: no frame for the page directory");
    u32 fb0 = fb_phys & PAGE_MASK, fb1 = (fb_phys + fb_size + PAGE_SIZE - 1) & PAGE_MASK;
    u32 cpu_edx; { u32 a = 1, b, c; __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(cpu_edx)); }
    int pse = (cpu_edx & (1u << 3)) != 0;
    for (u32 c = 0; c < ram_end; c += 0x400000u) {
        int overlaps_fb = fb_phys && c < fb1 && c + 0x400000u > fb0;
        if (c >= 0x400000u && pse && !overlaps_fb) {
            ((u32 *)kpd)[c >> 22] = c | PTE_P | PTE_W | PTE_PS;
            pse_chunks++;
            continue;
        }
        u32 end = c + 0x400000u < ram_end ? c + 0x400000u : ram_end;
        for (u32 a = c; a < end; a += PAGE_SIZE)
            if (vmm_map(kpd, a, a, PTE_W) < 0) kpanic("vmm: out of frames for kernel page tables");
    }
    if (pse_chunks) { u32 cr4; __asm__ volatile("mov %%cr4, %0" : "=r"(cr4)); cr4 |= 0x10u; __asm__ volatile("mov %0, %%cr4" :: "r"(cr4)); }
    if (fb_phys && fb0 >= ram_end) {
        if (fb0 < USER_TOP && fb1 > USER_BASE) kpanic("vmm: framebuffer %p overlaps user space", fb0);
        for (u32 a = fb0; a < fb1; a += PAGE_SIZE) vmm_map(kpd, a, a, PTE_W);
    }
    {
        u32 want = ((ram_total / 2) + 0x3FFFFFu) & ~0x3FFFFFu;
        if (want < 0x400000u) want = 0x400000u;
        if (want > 0x4000000u) want = 0x4000000u;
        u32 total = want + KSURF_SLOTS * KSURF_STRIDE;
        total = (total + 0x3FFFFFu) & ~0x3FFFFFu;
        u32 base = 0xD0000000u;
        for (int tries = 0; tries < 8 && fb_phys && base < fb1 && base + total > fb0; tries++) base += total;
        if (fb_phys && base < fb1 && base + total > fb0) kpanic("vmm: no room for the kernel heap region");
        if (base + total < base) kpanic("vmm: heap region runs past 4 GB");
        heap_base = base; heap_size = want; region_size = total;
        for (u32 va = base; va < base + total; va += 0x400000u)
            if (!get_pt(kpd, va, 1)) kpanic("vmm: out of frames for heap/surface page tables");
    }
    isr_register(14, pf_handler);
    vmm_switch(kpd);
    u32 cr0; __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u | 0x00010000u;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
    u32 *pt0 = (u32 *)(((u32 *)kpd)[0] & PAGE_MASK);
    pt0[0] = 0; invlpg(0);
    kprintf("vmm: paging on, identity %u MB (%u 4 MB chunks using 4 MB pages), fb %p (+%u KB)\n", ram_end >> 20, pse_chunks, fb_phys, fb_size >> 10);
}
