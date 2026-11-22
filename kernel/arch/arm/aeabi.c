#include "lib/types.h"
#include "lib/string.h"

static u32 udivmod(u32 n, u32 d, u32 *rem) {
    u32 q = 0;
    if (d == 0) { if (rem) *rem = n; return 0; }
    if (n >= d) {
        int sh = __builtin_clz(d) - __builtin_clz(n);
        d <<= sh;
        for (int i = 0; i <= sh; i++) {
            q <<= 1;
            if (n >= d) { n -= d; q |= 1; }
            d >>= 1;
        }
    }
    if (rem) *rem = n;
    return q;
}

u32 __aeabi_uidiv(u32 n, u32 d) { return udivmod(n, d, 0); }

u64 __aeabi_uidivmod(u32 n, u32 d) { u32 r; u32 q = udivmod(n, d, &r); return ((u64)r << 32) | q; }

int __aeabi_idiv(int n, int d) {
    int neg = (n < 0) ^ (d < 0);
    u32 q = udivmod(n < 0 ? 0u - (u32)n : (u32)n, d < 0 ? 0u - (u32)d : (u32)d, 0);
    return neg ? (int)(0u - q) : (int)q;
}

u64 __aeabi_idivmod(int n, int d) {
    u32 r;
    u32 q = udivmod(n < 0 ? 0u - (u32)n : (u32)n, d < 0 ? 0u - (u32)d : (u32)d, &r);
    if ((n < 0) ^ (d < 0)) q = 0u - q;
    if (n < 0) r = 0u - r;
    return ((u64)r << 32) | q;
}

void __aeabi_idiv0(void) {}
void __aeabi_ldiv0(void) {}

void __aeabi_memcpy(void *d, const void *s, size_t n)   { memcpy(d, s, n); }
void __aeabi_memcpy4(void *d, const void *s, size_t n)  { memcpy(d, s, n); }
void __aeabi_memcpy8(void *d, const void *s, size_t n)  { memcpy(d, s, n); }
void __aeabi_memmove(void *d, const void *s, size_t n)  { memmove(d, s, n); }
void __aeabi_memmove4(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove8(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memset(void *d, size_t n, int c)   { memset(d, c, n); }
void __aeabi_memset4(void *d, size_t n, int c)  { memset(d, c, n); }
void __aeabi_memset8(void *d, size_t n, int c)  { memset(d, c, n); }
void __aeabi_memclr(void *d, size_t n)   { memset(d, 0, n); }
void __aeabi_memclr4(void *d, size_t n)  { memset(d, 0, n); }
void __aeabi_memclr8(void *d, size_t n)  { memset(d, 0, n); }
