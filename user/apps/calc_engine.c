#include "calc_engine.h"

typedef unsigned long long u64_;
typedef unsigned int u32_;

static u64_ udiv(u64_ n, u64_ d, u64_ *rem) {
    u64_ q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= (u64_)1 << i; }
    }
    if (rem) *rem = r;
    return q;
}
static u64_ mag(cnum v) { return v < 0 ? (u64_)(-v) : (u64_)v; }
static cnum sgn(int neg, u64_ m) { return neg ? -(cnum)m : (cnum)m; }

int cn_apply(int op, cnum a, cnum b, cnum *r) {
    int neg = (a < 0) != (b < 0);
    switch (op) {
    case '+': *r = a + b; return (*r > CN_MAX || *r < -CN_MAX) ? -1 : 0;
    case '-': *r = a - b; return (*r > CN_MAX || *r < -CN_MAX) ? -1 : 0;
    case '*': {
        u64_ A = mag(a), B = mag(b);
        u32_ ai, af, bi, bf; u64_ t;
        ai = (u32_)udiv(A, CN_SCALE, &t); af = (u32_)t;
        bi = (u32_)udiv(B, CN_SCALE, &t); bf = (u32_)t;
        u64_ ii = (u64_)ai * bi;
        if (ii >= 1000000000ULL) return -1;
        u64_ lo = udiv((u64_)af * bf + CN_SCALE / 2, CN_SCALE, 0);
        u64_ m = ii * CN_SCALE + (u64_)ai * bf + (u64_)af * bi + lo;
        if (m > (u64_)CN_MAX) return -1;
        *r = sgn(neg, m); return 0; }
    case '/': {
        u64_ A = mag(a), B = mag(b), rem;
        if (!B) return -1;
        u64_ q = udiv(A, B, &rem);
        if (q >= 1000000000ULL) return -1;
        u64_ frac = 0;
        for (int i = 0; i < 6; i++) { u64_ d = udiv(rem * 10, B, &rem); frac = frac * 10 + d; }
        if (udiv(rem * 10, B, 0) >= 5) frac++;
        u64_ m = q * CN_SCALE + frac;
        if (m > (u64_)CN_MAX) return -1;
        *r = sgn(neg, m); return 0; }
    }
    return -1;
}

int cn_sqrt(cnum a, cnum *r) {
    if (a < 0) return -1;
    if (a == 0) { *r = 0; return 0; }
    cnum x = a > CN_SCALE ? a / 2 + CN_SCALE / 2 : CN_SCALE, q, s;
    for (int i = 0; i < 60; i++) {
        if (cn_apply('/', a, x, &q)) return -1;
        s = x + q;
        cnum nx = (cnum)udiv((u64_)s, 2, 0);
        if (nx == x) break;
        if ((nx > x ? nx - x : x - nx) <= 1) { x = nx; break; }
        x = nx;
    }
    *r = x; return 0;
}

void cn_format(cnum v, char *out, int max) {
    char t[32]; int n = 0, neg = v < 0; u64_ m = mag(v), fr;
    u64_ ip = udiv(m, CN_SCALE, &fr);
    char ib[16]; int il = 0;
    do { u64_ d; ip = udiv(ip, 10, &d); ib[il++] = (char)('0' + (int)d); } while (ip);
    if (neg) t[n++] = '-';
    while (il) t[n++] = ib[--il];
    if (fr) {
        char fb[8]; for (int i = 5; i >= 0; i--) { u64_ d; fr = udiv(fr, 10, &d); fb[i] = (char)('0' + (int)d); }
        int fl = 6; while (fl && fb[fl - 1] == '0') fl--;
        t[n++] = '.'; for (int i = 0; i < fl; i++) t[n++] = fb[i];
    }
    t[n] = 0;
    int i = 0; while (t[i] && i < max - 1) { out[i] = t[i]; i++; } out[i] = 0;
}

static cnum parse(const char *s) {
    int neg = 0; u64_ ip = 0, fp = 0; int fd = 0, dot = 0;
    if (*s == '-') { neg = 1; s++; }
    for (; *s; s++) {
        if (*s == '.') { dot = 1; continue; }
        if (*s < '0' || *s > '9') continue;
        if (!dot) ip = ip * 10 + (u64_)(*s - '0');
        else if (fd < 6) { fp = fp * 10 + (u64_)(*s - '0'); fd++; }
    }
    while (fd++ < 6) fp *= 10;
    return sgn(neg, ip * CN_SCALE + fp);
}

void calc_init(Calc *c) { c->acc = c->cur = 0; c->op = 0; c->entering = 0; c->err = 0; c->has_acc = 0; c->entry[0] = 0; c->elen = 0; c->edot = 0; }

static void commit(Calc *c) { if (c->entering) { c->cur = parse(c->entry); c->entering = 0; } }

static void run_pending(Calc *c) {
    if (c->op && c->has_acc) { if (cn_apply(c->op, c->acc, c->cur, &c->cur)) c->err = 1; }
    c->acc = c->cur; c->has_acc = 1;
}

void calc_key(Calc *c, int k) {
    if (c->err && k != 'C') return;
    if ((k >= '0' && k <= '9') || k == '.') {
        if (!c->entering) { c->entering = 1; c->entry[0] = 0; c->elen = 0; c->edot = 0; if (!c->op) c->has_acc = 0; }
        int digits = 0, dd = 0, seen_dot = 0;
        for (int i = 0; i < c->elen; i++) { if (c->entry[i] == '.') seen_dot = 1; else { digits++; if (seen_dot) dd++; } }
        if (k == '.') { if (c->edot) return; c->edot = 1; if (!c->elen) c->entry[c->elen++] = '0'; c->entry[c->elen++] = '.'; }
        else {
            if (c->edot ? dd >= 6 : digits >= 9) return;
            if (c->elen == 1 && c->entry[0] == '0') c->elen = 0;
            c->entry[c->elen++] = (char)k;
        }
        c->entry[c->elen] = 0;
        c->cur = parse(c->entry);
        return;
    }
    switch (k) {
    case '+': case '-': case '*': case '/':
        commit(c); run_pending(c); c->op = k; break;
    case '=': case '\n':
        commit(c); if (c->op) run_pending(c); c->op = 0; c->has_acc = 0; break;
    case 'C': calc_init(c); break;
    case 'n': commit(c); c->cur = -c->cur; break;
    case 'r': commit(c); if (cn_sqrt(c->cur, &c->cur)) c->err = 1; break;
    case '%': commit(c); if (cn_apply('/', c->cur, 100 * CN_SCALE, &c->cur)) c->err = 1; break;
    case '\b': if (c->entering && c->elen) { c->entry[--c->elen] = 0; c->edot = 0; for (int i = 0; i < c->elen; i++) if (c->entry[i] == '.') c->edot = 1; c->cur = parse(c->entry); } break;
    }
}

void calc_display(const Calc *c, char *out, int max) {
    if (c->err) { const char *e = "Error"; int i = 0; while (e[i] && i < max - 1) { out[i] = e[i]; i++; } out[i] = 0; return; }
    if (c->entering) { int i = 0; while (c->entry[i] && i < max - 1) { out[i] = c->entry[i]; i++; } out[i] = 0; return; }
    cn_format(c->cur, out, max);
}
