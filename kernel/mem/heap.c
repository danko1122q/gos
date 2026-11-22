#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "arch/io.h"
#include "arch/panic.h"

#define HMAGIC 0xB10C4EADu
#define HEAP_INIT_BYTES   (64u * 1024)
#define HEAP_GROW_MIN     (16u * 1024)
#define HEAP_SHRINK_MIN   (64u * 1024)
#define HEAP_SHRINK_KEEP  (16u * 1024)

typedef struct HB { u32 magic, size, used; struct HB *prev; } HB;

static HB *first;
static u32 hbase, hlimit, hend, used_bytes, peak_total;

static HB *next_of(HB *b) {
    u32 n = (u32)b + sizeof(HB) + b->size;
    return n >= hend ? NULL : (HB *)n;
}

static u32 heap_grow(u32 bytes) {
    if (bytes < HEAP_GROW_MIN) bytes = HEAP_GROW_MIN;
    bytes = (bytes + PAGE_SIZE - 1) & PAGE_MASK;
    if (bytes > hlimit - hend) return 0;
    u32 va = hend;
    for (u32 off = 0; off < bytes; off += PAGE_SIZE) {
        u32 f = pmm_alloc();
        if (!f || vmm_kmap(va + off, f) < 0) {
            if (f) pmm_free(f);
            for (u32 o = 0; o < off; o += PAGE_SIZE) pmm_free(vmm_kunmap(va + o));
            return 0;
        }
    }
    HB *last = NULL;
    if (first) for (HB *b = first; b; b = next_of(b)) last = b;
    hend += bytes;
    if (last && !last->used) last->size += bytes;
    else {
        HB *nb = (HB *)va;
        nb->magic = HMAGIC; nb->size = bytes - sizeof(HB); nb->used = 0; nb->prev = last;
        if (!first) first = nb;
    }
    if (hend - hbase > peak_total) peak_total = hend - hbase;
    return bytes;
}

static void heap_shrink(HB *b) {
    if (b->used || next_of(b) || b->size + sizeof(HB) < HEAP_SHRINK_MIN) return;
    u32 ne = ((u32)b + sizeof(HB) + HEAP_SHRINK_KEEP + PAGE_SIZE - 1) & PAGE_MASK;
    if (ne < hbase + HEAP_INIT_BYTES) ne = hbase + HEAP_INIT_BYTES;
    if (ne >= hend) return;
    b->size = ne - (u32)b - sizeof(HB);
    for (u32 va = ne; va < hend; va += PAGE_SIZE) pmm_free(vmm_kunmap(va));
    hend = ne;
}

void heap_init(void) {
    hbase = vmm_heap_base(); hlimit = hbase + vmm_heap_size(); hend = hbase;
    first = NULL; used_bytes = 0; peak_total = 0;
    if (!heap_grow(HEAP_INIT_BYTES)) kpanic("heap: no frame for the initial arena");
}

void *kmalloc(size_t n) {
    if (!n) n = 1;
    n = (n + 15) & ~15u;
    u32 fl = irq_save();
    for (int pass = 0; pass < 2; pass++) {
        for (HB *b = first; b; b = next_of(b)) {
            if (b->used || b->size < n) continue;
            if (b->size >= n + sizeof(HB) + 16) {
                HB *nb = (HB *)((u8 *)b + sizeof(HB) + n);
                nb->magic = HMAGIC; nb->size = b->size - n - sizeof(HB);
                nb->used = 0; nb->prev = b;
                HB *nn = next_of(nb); if (nn) nn->prev = nb;
                b->size = n;
            }
            b->used = 1; used_bytes += b->size + sizeof(HB);
            irq_restore(fl);
            return b + 1;
        }
        if (pass) break;
        HB *last = NULL;
        for (HB *b = first; b; b = next_of(b)) last = b;
        u32 need = last && !last->used ? n - last->size : n + sizeof(HB);
        if (!heap_grow(need)) break;
    }
    irq_restore(fl);
    return NULL;
}

void *kzalloc(size_t n) { void *p = kmalloc(n); if (p) memset(p, 0, n); return p; }

void kfree(void *p) {
    if (!p) return;
    u32 fl = irq_save();
    HB *b = (HB *)p - 1;
    if (b->magic != HMAGIC || !b->used) { kprintf("heap: invalid free %p\n", p); irq_restore(fl); return; }
    b->used = 0; used_bytes -= b->size + sizeof(HB);
    HB *n = next_of(b);
    if (n && !n->used) { b->size += sizeof(HB) + n->size; HB *nn = next_of(b); if (nn) nn->prev = b; }
    if (b->prev && !b->prev->used) {
        HB *pb = b->prev; pb->size += sizeof(HB) + b->size;
        HB *nn = next_of(pb); if (nn) nn->prev = pb;
        b = pb;
    }
    heap_shrink(b);
    irq_restore(fl);
}

void *krealloc(void *p, size_t n) {
    if (!p) return kmalloc(n);
    HB *b = (HB *)p - 1;
    if (n <= b->size) return p;
    void *q = kmalloc(n);
    if (!q) return NULL;
    memcpy(q, p, b->size);
    kfree(p);
    return q;
}

u32 heap_used(void)  { return used_bytes; }
u32 heap_total(void) { return hend - hbase; }
u32 heap_peak(void)  { return peak_total; }
