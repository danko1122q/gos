#include "arch/arch.h"
#include "lib/printf.h"
#include "lib/string.h"

int arch_cpuinfo(char *b, int m) {
    u32 a, bx, c, d; char v[13];
    __asm__ volatile("cpuid" : "=a"(a), "=b"(bx), "=c"(c), "=d"(d) : "a"(0));
    memcpy(v, &bx, 4); memcpy(v + 4, &d, 4); memcpy(v + 8, &c, 4); v[12] = 0;
    return ksnprintf(b, m, "vendor_id : %s\nmax_cpuid : %u\narch      : %s\n", v, a, GOS_ARCH_DESC);
}
