#include "gos.h"
#include <limits.h>
#include <stdio.h>

#define ATEXIT_MAX 32
static void (*exit_handlers[ATEXIT_MAX])(void);
static unsigned int exit_handler_count;
static int exit_running;
int atexit(void (*function)(void)) {
    if (!function) { errno = EINVAL; return -1; }
    if (exit_handler_count >= ATEXIT_MAX) { errno = ENOMEM; return -1; }
    exit_handlers[exit_handler_count++] = function;
    return 0;
}
void exit(int status) {
    if (!exit_running) {
        exit_running = 1;
        while (exit_handler_count) {
            void (*fn)(void) = exit_handlers[--exit_handler_count];
            if (fn) fn();
        }
    }
    (void)fflush(NULL);
    gos_exit(status);
}
void _Exit(int status) { gos_exit(status); }
void abort(void) { fprintf(stderr, "abort() called\n"); gos_exit(134); }
int abs(int v) { return v < 0 ? -v : v; }
long labs(long v) { return v < 0 ? -v : v; }
long long llabs(long long v) { return v < 0 ? -v : v; }
div_t div(int n, int d) { div_t r = { n / d, n % d }; return r; }
ldiv_t ldiv(long n, long d) { ldiv_t r = { n / d, n % d }; return r; }
lldiv_t lldiv(long long n, long long d) {
    lldiv_t out = { 0, 0 };
    if (!d) { errno = EDOM; return out; }
    unsigned long long un = n < 0 ? 0ULL - (unsigned long long)n : (unsigned long long)n;
    unsigned long long ud = d < 0 ? 0ULL - (unsigned long long)d : (unsigned long long)d;
    unsigned long long rem = 0, quot = 0;
    for (int bit = 63; bit >= 0; bit--) {
        rem = (rem << 1) | ((un >> bit) & 1ULL);
        if (rem >= ud) { rem -= ud; quot |= 1ULL << bit; }
    }
    out.quot = (long long)((n < 0) != (d < 0) ? 0ULL - quot : quot);
    out.rem = (long long)(n < 0 ? 0ULL - rem : rem);
    return out;
}
long atol(const char *s) { return strtol(s, NULL, 10); }
long long atoll(const char *s) { return strtoll(s, NULL, 10); }

static int digit_value(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = tolower((u8)c);
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}
long strtol(const char *s, char **endptr, int base) {
    const char *p = s;
    unsigned long value = 0, limit;
    int neg = 0, any = 0, overflow = 0;
    while (isspace((u8)*p)) p++;
    if (*p == '+' || *p == '-') { neg = *p == '-'; p++; }
    if (base && (base < 2 || base > 36)) { errno = EINVAL; if (endptr) *endptr = (char *)s; return 0; }
    if ((!base || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { p += 2; base = 16; }
    if (!base) base = *p == '0' ? 8 : 10;
    limit = neg ? 0x80000000u : 0x7fffffffu;
    for (;;) {
        int d = digit_value((u8)*p);
        if (d < 0 || d >= base) break;
        any = 1;
        if (value > limit / (unsigned)base || (value == limit / (unsigned)base && (unsigned)d > limit % (unsigned)base)) overflow = 1;
        else if (!overflow) value = value * (unsigned)base + (unsigned)d;
        p++;
    }
    if (endptr) *endptr = (char *)(any ? p : s);
    if (!any) return 0;
    if (overflow) { errno = ERANGE; return neg ? (long)0x80000000u : LONG_MAX; }
    if (neg) return value == 0x80000000u ? (long)0x80000000u : -(long)value;
    return (long)value;
}
unsigned long strtoul(const char *s, char **endptr, int base) {
    const char *p = s; unsigned long value = 0; int neg = 0, any = 0, overflow = 0;
    while (isspace((u8)*p)) p++;
    if (*p == '+' || *p == '-') { neg = *p == '-'; p++; }
    if (base && (base < 2 || base > 36)) { errno = EINVAL; if (endptr) *endptr = (char *)s; return 0; }
    if ((!base || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { p += 2; base = 16; }
    if (!base) base = *p == '0' ? 8 : 10;
    for (;;) {
        int d = digit_value((u8)*p);
        if (d < 0 || d >= base) break;
        any = 1;
        if (value > UINT_MAX / (unsigned)base || (value == UINT_MAX / (unsigned)base && (unsigned)d > UINT_MAX % (unsigned)base)) overflow = 1;
        else if (!overflow) value = value * (unsigned)base + (unsigned)d;
        p++;
    }
    if (endptr) *endptr = (char *)(any ? p : s);
    if (!any) return 0;
    if (overflow) { errno = ERANGE; return UINT_MAX; }
    return neg ? 0u - value : value;
}

static unsigned long long divmod_u64(unsigned long long value, unsigned int base, unsigned int *rem) {
    unsigned long long quotient = 0;
    unsigned int r = 0;
    for (int bit = 63; bit >= 0; bit--) {
        r = (r << 1) | (unsigned int)((value >> bit) & 1ULL);
        if (r >= base) { r -= base; quotient |= 1ULL << bit; }
    }
    *rem = r;
    return quotient;
}
static unsigned long long parse_u64(const char *s, char **endptr, int base, int *negative, int *overflow, unsigned long long limit) {
    const char *p = s;
    unsigned long long value = 0;
    int any = 0;
    *negative = 0; *overflow = 0;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { *negative = *p == '-'; p++; }
    if (base && (base < 2 || base > 36)) {
        errno = EINVAL; if (endptr) *endptr = (char *)s; return 0;
    }
    if ((!base || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { p += 2; base = 16; }
    if (!base) base = *p == '0' ? 8 : 10;
    for (;;) {
        int d = digit_value((unsigned char)*p);
        if (d < 0 || d >= base) break;
        any = 1;
        unsigned int rem;
        unsigned long long q = divmod_u64(limit, (unsigned int)base, &rem);
        if (value > q || (value == q && (unsigned int)d > rem)) *overflow = 1;
        if (!*overflow) value = value * (unsigned int)base + (unsigned int)d;
        p++;
    }
    if (endptr) *endptr = (char *)(any ? p : s);
    if (!any) return 0;
    if (*overflow) { errno = ERANGE; return limit; }
    return value;
}
long long strtoll(const char *s, char **endptr, int base) {
    int neg, overflow;
    unsigned long long limit = 0x7fffffffffffffffULL;
    const char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '-') limit = 0x8000000000000000ULL;
    unsigned long long v = parse_u64(s, endptr, base, &neg, &overflow, limit);
    if (overflow) return neg ? (long long)0x8000000000000000ULL : 0x7fffffffffffffffLL;
    if (!neg) return (long long)v;
    if (v == 0x8000000000000000ULL) return (long long)0x8000000000000000ULL;
    return -(long long)v;
}
unsigned long long strtoull(const char *s, char **endptr, int base) {
    int neg, overflow;
    unsigned long long v = parse_u64(s, endptr, base, &neg, &overflow, 0xffffffffffffffffULL);
    if (overflow) return 0xffffffffffffffffULL;
    return neg ? 0ULL - v : v;
}
intmax_t strtoimax(const char *s, char **endptr, int base) { return (intmax_t)strtoll(s, endptr, base); }
uintmax_t strtoumax(const char *s, char **endptr, int base) { return (uintmax_t)strtoull(s, endptr, base); }
intmax_t imaxabs(intmax_t value) { return llabs(value); }
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom) {
    lldiv_t r = lldiv(numer, denom);
    imaxdiv_t out = { r.quot, r.rem };
    return out;
}

static unsigned int random_state = 1;
void srand(unsigned int seed) { random_state = seed ? seed : 1; }
int rand(void) { random_state = random_state * 1103515245u + 12345u; return (int)((random_state >> 1) & 0x7fffffffu); }
int rand_r(unsigned int *seed) { if (!seed) { errno = EINVAL; return 0; } *seed = *seed * 1103515245u + 12345u; return (int)((*seed >> 1) & 0x7fffffffu); }

static void swap_bytes(unsigned char *a, unsigned char *b, size_t n) {
    while (n--) { unsigned char t = *a; *a++ = *b; *b++ = t; }
}
static void sift_down(unsigned char *base, size_t start, size_t end, size_t width,
                      int (*cmp)(const void *, const void *)) {
    size_t root = start;
    while (end >= 2 && root <= (end - 2) / 2) {
        size_t child = root * 2 + 1;
        if (child + 1 < end && cmp(base + child * width, base + (child + 1) * width) < 0) child++;
        if (cmp(base + root * width, base + child * width) >= 0) return;
        swap_bytes(base + root * width, base + child * width, width);
        root = child;
    }
}
void qsort(void *array, size_t n, size_t width, int (*cmp)(const void *, const void *)) {
    unsigned char *base = array;
    if (!base || !cmp || !width || n < 2 || n > 0xffffffffu / width) return;
    for (size_t start = n / 2; start > 0; ) sift_down(base, --start, n, width, cmp);
    for (size_t end = n - 1; end > 0; end--) {
        swap_bytes(base, base + end * width, width);
        sift_down(base, 0, end, width, cmp);
    }
}
void *bsearch(const void *key, const void *array, size_t n, size_t width,
              int (*cmp)(const void *, const void *)) {
    const unsigned char *base = array;
    if (!key || !base || !cmp || !width || n > 0xffffffffu / width) return NULL;
    while (n) {
        size_t mid = n / 2;
        const void *item = base + mid * width;
        int r = cmp(key, item);
        if (!r) return (void *)item;
        if (r < 0) n = mid;
        else { base += (mid + 1) * width; n -= mid + 1; }
    }
    return NULL;
}
