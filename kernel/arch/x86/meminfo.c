#include "mem/heap.h"

static u32 mm_end_kb, mm_base_kb, mm_src;
static void mem_read_bootinfo(void) {
    if (mm_end_kb) return;
    u32 e, b, a0 = 0x610, a1 = 0x614, a2 = 0x61C;
    __asm__ volatile("movzbl (%1), %0" : "=r"(mm_src) : "r"(a2) : "memory");
    __asm__ volatile("movl (%1), %0" : "=r"(e) : "r"(a0) : "memory");
    __asm__ volatile("movl (%1), %0" : "=r"(b) : "r"(a1) : "memory");
    mm_end_kb = e ? e : 16 * 1024;
    mm_base_kb = b >= 1024 && b < mm_end_kb ? b : 1024;
}
u32 mem_total_kb(void) { mem_read_bootinfo(); return mm_end_kb; }
u32 mem_base_kb(void)  { mem_read_bootinfo(); return mm_base_kb; }
const char *mem_map_source(void) { mem_read_bootinfo(); return mm_src == 1 ? "E820" : "E801"; }
