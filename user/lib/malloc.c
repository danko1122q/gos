#include "gos.h"

#define HMAGIC 0xB10C4EADu
#define ALIGNMENT 16u
#define CHUNK_SIZE 0x10000u

typedef struct HB {
    u32 magic;
    u32 size;
    u32 used;
    struct HB *prev;
} HB;

static u8 *hbase;
static u32 hsize;

static u32 align_up(u32 n) {
    if (n > 0xffffffffu - (ALIGNMENT - 1u)) return 0;
    return (n + (ALIGNMENT - 1u)) & ~(ALIGNMENT - 1u);
}
static HB *first(void) { return hsize ? (HB *)hbase : NULL; }
static u8 *heap_end(void) { return hbase + hsize; }
static HB *next_of(HB *b) {
    u8 *p = (u8 *)b;
    u8 *end = heap_end();
    if (p < hbase || p > end || (u32)(end - p) < sizeof(HB)) return NULL;
    if (b->size > (u32)(end - p) - sizeof(HB)) return NULL;
    p += sizeof(HB) + b->size;
    return p < end ? (HB *)p : NULL;
}
static int extend(u32 need) {
    if (need > 0xffff0000u - sizeof(HB)) return -1;
    u32 total = need + sizeof(HB);
    if (total > 0xffffu) {
        u32 remainder = total & 0xffffu;
        if (remainder) {
            u32 extra = 0x10000u - remainder;
            if (total > 0xffffffffu - extra) return -1;
            total += extra;
        }
    } else total = CHUNK_SIZE;
    if (total < need + sizeof(HB) || hsize > 0xffffffffu - total) return -1;
    u32 cur = (u32)gos_brk(0);
    if (!hbase) {
        u32 aligned = align_up(cur);
        if (!aligned) return -1;
        hbase = (u8 *)aligned;
    }
    if ((u32)hbase > 0xffffffffu - hsize - total) return -1;
    u32 want = (u32)hbase + hsize + total;
    if ((u32)gos_brk(want) != want) return -1;

    HB *last = NULL;
    for (HB *b = first(); b; b = next_of(b)) last = b;
    HB *nb = (HB *)(hbase + hsize);
    nb->magic = HMAGIC;
    nb->size = total - sizeof(HB);
    nb->used = 0;
    nb->prev = last;
    hsize += total;
    if (last && last->magic == HMAGIC && !last->used)
        last->size += sizeof(HB) + nb->size;
    return 0;
}

void *kmalloc(size_t n) {
    if (!n) n = 1;
    if (n > 0xffff0000u - (ALIGNMENT - 1u)) return NULL;
    n = align_up(n);
    for (;;) {
        for (HB *b = first(); b; b = next_of(b)) {
            if (b->magic != HMAGIC || b->used || b->size < n) continue;
            if (b->size - n >= sizeof(HB) + ALIGNMENT) {
                HB *nb = (HB *)((u8 *)b + sizeof(HB) + n);
                nb->magic = HMAGIC;
                nb->size = b->size - n - sizeof(HB);
                nb->used = 0;
                nb->prev = b;
                HB *nn = next_of(nb);
                if (nn) nn->prev = nb;
                b->size = n;
            }
            b->used = 1;
            return (void *)(b + 1);
        }
        if (extend(n)) return NULL;
    }
}

void *kzalloc(size_t n) {
    void *p = kmalloc(n);
    if (p) memset(p, 0, n);
    else errno = ENOMEM;
    return p;
}

void kfree(void *p) {
    if (!p || !hbase || (u32)p < (u32)hbase + sizeof(HB) || (u32)p >= (u32)heap_end()) return;
    HB *b = (HB *)p - 1;
    if ((u32)b < (u32)hbase || (u32)b + sizeof(HB) > (u32)heap_end()) return;
    if (b->magic != HMAGIC || !b->used || b->size > (u32)(heap_end() - (u8 *)b) - sizeof(HB)) return;
    b->used = 0;
    HB *n = next_of(b);
    if (n && n->magic == HMAGIC && !n->used) {
        b->size += sizeof(HB) + n->size;
        n->magic = 0;
        HB *nn = next_of(b);
        if (nn) nn->prev = b;
    }
    if (b->prev && (u8 *)b->prev >= hbase && b->prev->magic == HMAGIC && !b->prev->used) {
        HB *pb = b->prev;
        pb->size += sizeof(HB) + b->size;
        b->magic = 0;
        HB *nn = next_of(pb);
        if (nn) nn->prev = pb;
    }
}

void *krealloc(void *p, size_t n) {
    if (!p) return kmalloc(n);
    if (!n) { kfree(p); return NULL; }
    if ((u32)p < (u32)hbase + sizeof(HB) || (u32)p >= (u32)heap_end()) return NULL;
    HB *b = (HB *)p - 1;
    if (b->magic != HMAGIC || !b->used) return NULL;
    if (n <= b->size) return p;
    void *q = kmalloc(n);
    if (!q) return NULL;
    memcpy(q, p, b->size);
    kfree(p);
    return q;
}

void *malloc(size_t n) { void *p = kmalloc(n); if (!p) errno = ENOMEM; return p; }
void free(void *p) { kfree(p); }
void *realloc(void *p, size_t n) { void *q = krealloc(p, n); if (!q && n) errno = ENOMEM; return q; }
void *calloc(size_t count, size_t size) {
    if (size && count > 0xffffffffu / size) { errno = ENOMEM; return NULL; }
    size_t n = count * size;
    void *p = kmalloc(n);
    if (p) memset(p, 0, n);
    else errno = ENOMEM;
    return p;
}
