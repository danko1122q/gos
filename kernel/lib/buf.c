#include "lib/buf.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mem/heap.h"

#define BUF_MAX (1 << 20)

void buf_init(Buf *b) { b->p = NULL; b->len = b->cap = 0; }
void buf_free(Buf *b) { kfree(b->p); buf_init(b); }

int buf_put(Buf *b, const void *d, int n) {
    if (n <= 0) return 0;
    if (b->len + n + 1 > b->cap) {
        int nc = b->cap ? b->cap : 256;
        while (nc < b->len + n + 1) nc *= 2;
        if (nc > BUF_MAX) return -1;
        char *np = krealloc(b->p, nc);
        if (!np) return -1;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, d, n);
    b->len += n; b->p[b->len] = 0;
    return 0;
}
int buf_putc(Buf *b, char c) { return buf_put(b, &c, 1); }
int buf_puts(Buf *b, const char *s) { return buf_put(b, s, (int)strlen(s)); }
int buf_printf(Buf *b, const char *fmt, ...) {
    char t[512];
    va_list ap; va_start(ap, fmt);
    int n = kvsnprintf(t, sizeof t, fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof t) n = sizeof t - 1;
    return buf_put(b, t, n);
}
