#include "lib/printf.h"
#include "lib/string.h"

typedef struct { char *buf; size_t size; int count; } Out;
enum { LEN_DEF, LEN_HH, LEN_H, LEN_L, LEN_LL, LEN_Z, LEN_T, LEN_J };

static __attribute__((noinline)) void emit(Out *o, char c) {
    if (o->buf && o->size && o->count >= 0 && (size_t)o->count < o->size - 1) o->buf[o->count] = c;
    if (o->count < 2147483647) o->count++;
}
static void repeat(Out *o, char c, int n) { while (n-- > 0) emit(o, c); }
static unsigned long long divmod_small(unsigned long long value, unsigned int base, unsigned int *rem) {
    unsigned long long quotient = 0;
    unsigned int r = 0;
    for (int bit = 63; bit >= 0; bit--) {
        r = (r << 1) | (unsigned int)((value >> bit) & 1ULL);
        if (r >= base) { r -= base; quotient |= 1ULL << bit; }
    }
    *rem = r;
    return quotient;
}
static int make_digits(char *dst, unsigned long long value, unsigned int base, int upper) {
    const char *digits = upper ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
    int n = 0;
    do { unsigned int rem; value = divmod_small(value, base, &rem); dst[n++] = digits[rem]; } while (value && n < 65);
    return n;
}
static unsigned long long get_unsigned(va_list *ap, int len) {
    switch (len) {
    case LEN_L: return va_arg(*ap, unsigned long);
    case LEN_LL: case LEN_J: return va_arg(*ap, unsigned long long);
    case LEN_Z: case LEN_T: return va_arg(*ap, size_t);
    case LEN_H: return (unsigned short)va_arg(*ap, unsigned int);
    case LEN_HH: return (unsigned char)va_arg(*ap, unsigned int);
    default: return va_arg(*ap, unsigned int);
    }
}
static long long get_signed(va_list *ap, int len) {
    switch (len) {
    case LEN_L: return va_arg(*ap, long);
    case LEN_LL: case LEN_J: return va_arg(*ap, long long);
    case LEN_Z: case LEN_T: return va_arg(*ap, s32);
    case LEN_H: return (short)va_arg(*ap, int);
    case LEN_HH: return (signed char)va_arg(*ap, int);
    default: return va_arg(*ap, int);
    }
}
static void number(Out *o, unsigned long long value, int negative, unsigned int base,
                   int upper, int width, int precision, int left, int zero,
                   int plus, int space, int alt, int pointer) {
    char digits[65];
    int nd = (precision == 0 && value == 0) ? 0 : make_digits(digits, value, base, upper);
    char sign = negative ? '-' : plus ? '+' : space ? ' ' : 0;
    char prefix[2]; int np = 0;
    if (pointer) { prefix[np++] = '0'; prefix[np++] = 'x'; }
    else if (alt && base == 16 && value) { prefix[np++] = '0'; prefix[np++] = upper ? 'X' : 'x'; }
    else if (alt && base == 8 && (!nd || digits[nd - 1] != '0')) prefix[np++] = '0';
    int zeros = precision > nd ? precision - nd : 0;
    int body = nd + zeros + np + (sign != 0);
    int pad = width > body ? width - body : 0;
    if (zero && !left && precision < 0) { zeros += pad; pad = 0; }
    if (!left) repeat(o, ' ', pad);
    if (sign) emit(o, sign);
    for (int i = 0; i < np; i++) emit(o, prefix[i]);
    repeat(o, '0', zeros);
    while (nd) emit(o, digits[--nd]);
    if (left) repeat(o, ' ', pad);
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    Out o = { buf, size, 0 };
    if (!fmt || (!buf && size)) return -1;
    while (*fmt) {
        if (*fmt != '%') { emit(&o, *fmt++); continue; }
        fmt++;
        if (*fmt == '%') { emit(&o, '%'); fmt++; continue; }
        int left = 0, plus = 0, space = 0, alt = 0, zero = 0;
        int width = 0, precision = -1, len = LEN_DEF;
        int flags = 1;
        while (flags) {
            switch (*fmt) {
            case '-': left = 1; fmt++; break;
            case '+': plus = 1; fmt++; break;
            case ' ': space = 1; fmt++; break;
            case '#': alt = 1; fmt++; break;
            case '0': zero = 1; fmt++; break;
            default: flags = 0; break;
            }
        }
        if (*fmt == '*') {
            width = va_arg(ap, int); fmt++;
            if (width < 0) { left = 1; width = width == (-2147483647 - 1) ? 2147483647 : -width; }
        } else while (*fmt >= '0' && *fmt <= '9') { if (width < 100000) width = width * 10 + (*fmt - '0'); fmt++; }
        if (*fmt == '.') {
            fmt++; precision = 0;
            if (*fmt == '*') { precision = va_arg(ap, int); fmt++; if (precision < 0) precision = -1; }
            else while (*fmt >= '0' && *fmt <= '9') { if (precision < 100000) precision = precision * 10 + (*fmt - '0'); fmt++; }
        }
        if (*fmt == 'h') { fmt++; len = LEN_H; if (*fmt == 'h') { fmt++; len = LEN_HH; } }
        else if (*fmt == 'l') { fmt++; len = LEN_L; if (*fmt == 'l') { fmt++; len = LEN_LL; } }
        else if (*fmt == 'z') { fmt++; len = LEN_Z; }
        else if (*fmt == 't') { fmt++; len = LEN_T; }
        else if (*fmt == 'j') { fmt++; len = LEN_J; }
        char conv = *fmt ? *fmt++ : 0;
        switch (conv) {
        case 'c': {
            char c = (char)va_arg(ap, int);
            int pad = width > 1 ? width - 1 : 0;
            if (!left) repeat(&o, ' ', pad);
            emit(&o, c);
            if (left) repeat(&o, ' ', pad);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *); if (!s) s = "(null)";
            size_t n = precision >= 0 ? strnlen(s, (size_t)precision) : strlen(s);
            int pad = width > (int)n ? width - (int)n : 0;
            if (!left) repeat(&o, ' ', pad);
            for (size_t i = 0; i < n; i++) emit(&o, s[i]);
            if (left) repeat(&o, ' ', pad);
            break;
        }
        case 'd': case 'i': {
            long long v = get_signed(&ap, len);
            unsigned long long mag = v < 0 ? 0ULL - (unsigned long long)v : (unsigned long long)v;
            number(&o, mag, v < 0, 10, 0, width, precision, left, zero, plus, space, 0, 0);
            break;
        }
        case 'u': case 'o': case 'x': case 'X': {
            unsigned int base = conv == 'o' ? 8 : (conv == 'u' ? 10 : 16);
            number(&o, get_unsigned(&ap, len), 0, base, conv == 'X', width, precision, left, zero, 0, 0, alt, 0);
            break;
        }
        case 'p': {
            unsigned long long v = (u32)va_arg(ap, void *);
            number(&o, v, 0, 16, 0, width, precision < 0 ? 8 : precision, left, zero, 0, 0, 0, 1);
            break;
        }
        case 'n': {
            void *p = va_arg(ap, void *);
            if (p) {
                if (len == LEN_HH) *(signed char *)p = (signed char)o.count;
                else if (len == LEN_H) *(short *)p = (short)o.count;
                else if (len == LEN_L) *(long *)p = o.count;
                else if (len == LEN_LL || len == LEN_J) *(long long *)p = o.count;
                else if (len == LEN_Z || len == LEN_T) *(s32 *)p = o.count;
                else *(int *)p = o.count;
            }
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
            (void)va_arg(ap, double);
            emit(&o, '<'); emit(&o, 'n'); emit(&o, 'o'); emit(&o, 'f'); emit(&o, 'p'); emit(&o, '>');
            break;
        case 0: emit(&o, '%'); fmt--; break;
        default: emit(&o, '%'); if (conv) emit(&o, conv); break;
        }
    }
    if (size) {
        size_t end = o.count < 0 ? 0u : (size_t)o.count;
        if (end >= size) end = size - 1;
        buf[end] = 0;
    }
    return o.count;
}
int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = kvsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
