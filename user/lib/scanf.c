#include "gos.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef int (*scan_get_fn)(void *);
typedef void (*scan_unget_fn)(int, void *);
typedef struct { FILE *file; const char *text; size_t pos; } ScanInput;

static int file_get(void *p) { return fgetc(((ScanInput *)p)->file); }
static void file_unget(int c, void *p) { if (c != EOF) (void)ungetc(c, ((ScanInput *)p)->file); }
static int text_get(void *p) {
    ScanInput *in = p;
    int c = (unsigned char)in->text[in->pos];
    if (c) in->pos++;
    else c = EOF;
    return c;
}
static void text_unget(int c, void *p) { ScanInput *in = p; if (c != EOF && in->pos) in->pos--; }
static int scan_space(int c) { return c != EOF && isspace((unsigned char)c); }
static int scan_match(char c, const char *set, int invert) {
    int found = 0;
    while (*set) {
        if (set[1] == '-' && set[2]) {
            char lo = set[0], hi = set[2];
            if ((c >= lo && c <= hi) || (c >= hi && c <= lo)) found = 1;
            set += 3;
        } else { if (c == *set) found = 1; set++; }
    }
    return invert ? !found : found;
}
static int scan_core(scan_get_fn get, scan_unget_fn unget, void *ctx, const char *fmt, va_list ap) {
    int assigned = 0, consumed = 0, input_failure = 0;
    while (*fmt) {
        if (isspace((unsigned char)*fmt)) {
            while (isspace((unsigned char)*fmt)) fmt++;
            int c; do { c = get(ctx); if (c != EOF) consumed++; } while (scan_space(c));
            if (c != EOF) { unget(c, ctx); consumed--; }
            continue;
        }
        if (*fmt != '%') {
            int c = get(ctx);
            if (c == EOF) { input_failure = 1; break; }
            consumed++;
            if (c != (unsigned char)*fmt) { unget(c, ctx); consumed--; break; }
            fmt++; continue;
        }
        fmt++;
        if (*fmt == '%') {
            int c = get(ctx);
            if (c == EOF) { input_failure = 1; break; }
            consumed++;
            if (c != '%') { unget(c, ctx); consumed--; break; }
            fmt++; continue;
        }
        int suppress = 0, width = 0, length = 0, width_given = 0;
        if (*fmt == '*') { suppress = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width_given = 1; if (width < 100000) width = width * 10 + (*fmt - '0'); fmt++; }
        if (!width_given || !width) width = INT_MAX;
        if (*fmt == 'h') { fmt++; length = 1; if (*fmt == 'h') { fmt++; length = 2; } }
        else if (*fmt == 'l') { fmt++; length = 3; if (*fmt == 'l') { fmt++; length = 4; } }
        else if (*fmt == 'z') { fmt++; length = 5; }
        char conv = *fmt ? *fmt++ : 0;
        if (conv == 'n') {
            if (!suppress) {
                if (length == 1) *va_arg(ap, short *) = (short)consumed;
                else if (length == 2) *va_arg(ap, signed char *) = (signed char)consumed;
                else if (length == 3) *va_arg(ap, long *) = consumed;
                else *va_arg(ap, int *) = consumed;
            }
            continue;
        }
        if (conv != 'c' && conv != '[') {
            int c; do { c = get(ctx); if (c != EOF) consumed++; } while (scan_space(c));
            if (c != EOF) { unget(c, ctx); consumed--; }
            else { input_failure = 1; break; }
        }
        if (conv == 'c') {
            if (!width_given) width = 1;
            char *out = suppress ? NULL : va_arg(ap, char *);
            int n = 0;
            while (n < width) {
                int c = get(ctx);
                if (c == EOF) { input_failure = 1; break; }
                consumed++; if (out) out[n] = (char)c; n++;
            }
            if (n != width) break;
            if (!suppress) assigned++;
            continue;
        }
        if (conv == 's' || conv == '[') {
            char *out = suppress ? NULL : va_arg(ap, char *);
            int invert = 0; char set[128]; int ns = 0;
            if (conv == '[') {
                if (*fmt == '^') { invert = 1; fmt++; }
                if (*fmt == ']') set[ns++] = *fmt++;
                while (*fmt && *fmt != ']' && ns < (int)sizeof set - 1) set[ns++] = *fmt++;
                if (*fmt != ']') break;
                fmt++; set[ns] = 0;
            }
            int n = 0, c;
            while (n < width) {
                c = get(ctx);
                if (c == EOF) { input_failure = 1; break; }
                int match = conv == 's' ? !scan_space(c) : scan_match((char)c, set, invert);
                if (!match) { unget(c, ctx); break; }
                consumed++; if (out) out[n] = (char)c; n++;
            }
            if (!n) break;
            if (out) out[n] = 0;
            if (!suppress) assigned++;
            continue;
        }
        if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'o' || conv == 'x' || conv == 'X' || conv == 'p') {
            char token[64]; int n = 0, c;
            while (n < width) {
                c = get(ctx); if (c == EOF) { input_failure = 1; break; }
                int ok = (c >= '0' && c <= '9') || ((conv == 'i' || conv == 'x' || conv == 'X' || conv == 'p') && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == 'x' || c == 'X'));
                if ((c == '+' || c == '-') && n == 0) ok = 1;
                if (!ok || (conv == 'o' && (c < '0' || c > '7'))) { unget(c, ctx); break; }
                if (n < (int)sizeof token - 1) token[n] = (char)c;
                n++; consumed++;
            }
            if (n >= (int)sizeof token) n = (int)sizeof token - 1;
            token[n] = 0;
            if (!n) break;
            char *end; int base = conv == 'i' ? 0 : conv == 'o' ? 8 : (conv == 'x' || conv == 'X' || conv == 'p') ? 16 : 10;
            int sign = conv == 'd' || conv == 'i';
            if (sign) {
                long v = strtol(token, &end, base); if (end == token) break;
                if (!suppress) {
                    if (length == 1) *va_arg(ap, short *) = (short)v;
                    else if (length == 2) *va_arg(ap, signed char *) = (signed char)v;
                    else if (length == 3) *va_arg(ap, long *) = v;
                    else if (length == 4) *va_arg(ap, long long *) = (long long)v;
                    else *va_arg(ap, int *) = (int)v;
                }
            } else {
                unsigned long v = strtoul(token, &end, base); if (end == token) break;
                if (!suppress) {
                    if (conv == 'p') *va_arg(ap, void **) = (void *)(u32)v;
                    else if (length == 1) *va_arg(ap, unsigned short *) = (unsigned short)v;
                    else if (length == 2) *va_arg(ap, unsigned char *) = (unsigned char)v;
                    else if (length == 3) *va_arg(ap, unsigned long *) = v;
                    else if (length == 4) *va_arg(ap, unsigned long long *) = (unsigned long long)v;
                    else *va_arg(ap, unsigned int *) = (unsigned int)v;
                }
            }
            if (!suppress) assigned++;
            continue;
        }
        break;
    }
    if (!assigned && input_failure) return EOF;
    return assigned;
}
int vfscanf(FILE *f, const char *fmt, va_list ap) { ScanInput in = { .file = f }; return scan_core(file_get, file_unget, &in, fmt, ap); }
int fscanf(FILE *f, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfscanf(f, fmt, ap); va_end(ap); return r; }
int scanf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfscanf(stdin, fmt, ap); va_end(ap); return r; }
int vsscanf(const char *str, const char *fmt, va_list ap) { if (!str) return EOF; ScanInput in = { .text = str }; return scan_core(text_get, text_unget, &in, fmt, ap); }
int sscanf(const char *str, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsscanf(str, fmt, ap); va_end(ap); return r; }

int vscanf(const char *fmt, va_list ap) { return vfscanf(stdin, fmt, ap); }
