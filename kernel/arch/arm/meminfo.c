#include "mem/heap.h"

extern u32 arm_atags;
#define ATAG_CORE 0x54410001u
#define ATAG_MEM  0x54410002u
#define DEFAULT_RAM_KB (32u * 1024u)

static u32 end_kb;
static const char *src = "default";

static void mem_read_bootinfo(void) {
    if (end_kb) return;
    end_kb = DEFAULT_RAM_KB;
    const u32 *t = (const u32 *)arm_atags;
    if (!t || (u32)t & 3 || t[1] != ATAG_CORE) return;
    for (int n = 0; n < 32 && t[0] >= 2; n++, t += t[0]) {
        if (t[1] == ATAG_MEM && t[0] >= 4 && t[3] == 0 && t[2] >= 0x100000u) {
            end_kb = t[2] / 1024u; src = "ATAG"; return;
        }
        if (t[1] == 0) break;
    }
}

u32 mem_total_kb(void)  { mem_read_bootinfo(); return end_kb; }
u32 mem_base_kb(void)   { return 1024; }
const char *mem_map_source(void) { mem_read_bootinfo(); return src; }
