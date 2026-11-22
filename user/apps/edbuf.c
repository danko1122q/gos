#include "edbuf.h"
#ifdef EDBUF_HOST
#include <stdlib.h>
#include <string.h>
#define ed_realloc realloc
#define ed_free    free
#else
#include "gos.h"
#define ed_realloc krealloc
#define ed_free    kfree
#endif

#define MAX_OPS    512
#define MAX_UBYTES (256 * 1024)
#define MAX_RUN    128

void eb_init(EdBuf *b, int maxlen) {
    memset(b, 0, sizeof *b);
    b->max = maxlen; b->nl = 1;
    b->ls = ed_realloc(NULL, sizeof(int)); if (b->ls) b->ls[0] = 0;
    b->clean_ok = 1;
}

static void op_free(EdOp *o) { ed_free(o->txt); o->txt = NULL; }

static void stack_clear(EdOp **s, int *n, int *c) {
    for (int i = 0; i < *n; i++) op_free(&(*s)[i]);
    *n = 0;
    (void)c;
}

void eb_free(EdBuf *b) {
    stack_clear(&b->u, &b->nu, &b->cu); stack_clear(&b->r, &b->nr, &b->cr);
    ed_free(b->u); ed_free(b->r); ed_free(b->t); ed_free(b->ls);
    memset(b, 0, sizeof *b);
}

static void relines(EdBuf *b) {
    int n = 1;
    for (int i = 0; i < b->len; i++) if (b->t[i] == '\n') n++;
    int *ls = ed_realloc(b->ls, (size_t)n * sizeof(int));
    if (!ls) return;
    b->ls = ls; b->nl = n; ls[0] = 0;
    int k = 1;
    for (int i = 0; i < b->len; i++) if (b->t[i] == '\n') ls[k++] = i + 1;
}

static int grow(EdBuf *b, int need) {
    if (b->len + need + 1 <= b->cap) return 0;
    int nc = b->cap ? b->cap : 1024;
    while (nc < b->len + need + 1) nc *= 2;
    char *p = ed_realloc(b->t, (size_t)nc);
    if (!p) return -1;
    b->t = p; b->cap = nc;
    return 0;
}

static int raw_insert(EdBuf *b, int pos, const char *s, int n) {
    if (pos < 0 || pos > b->len || n < 0 || b->len + n > b->max) return -1;
    if (grow(b, n)) return -1;
    memmove(b->t + pos + n, b->t + pos, (size_t)(b->len - pos));
    memcpy(b->t + pos, s, (size_t)n);
    b->len += n; b->t[b->len] = 0;
    relines(b);
    return 0;
}
static void raw_erase(EdBuf *b, int pos, int n) {
    memmove(b->t + pos, b->t + pos + n, (size_t)(b->len - pos - n));
    b->len -= n; b->t[b->len] = 0;
    relines(b);
}

int eb_load(EdBuf *b, const char *s, int n) {
    if (n < 0 || n > b->max) return -1;
    if (grow(b, n)) return -1;
    if (n) memcpy(b->t, s, (size_t)n);
    b->len = n; b->t[n] = 0;
    stack_clear(&b->u, &b->nu, &b->cu); stack_clear(&b->r, &b->nr, &b->cr);
    b->ubytes = 0; b->grp = 0;
    relines(b);
    eb_mark_clean(b);
    return 0;
}

static int push_op(EdOp **s, int *n, int *c, const EdOp *o) {
    if (*n == *c) {
        int nc = *c ? *c * 2 : 32;
        EdOp *p = ed_realloc(*s, (size_t)nc * sizeof(EdOp));
        if (!p) return -1;
        *s = p; *c = nc;
    }
    (*s)[(*n)++] = *o;
    return 0;
}

static void clear_redo(EdBuf *b) {
    if (!b->nr) return;
    if (b->clean_ok && b->clean_at > b->nu) b->clean_ok = 0;
    stack_clear(&b->r, &b->nr, &b->cr);
}

static void drop_oldest(EdBuf *b) {
    int g = b->u[0].grp, k = 0;
    do { b->ubytes -= b->u[k].len; op_free(&b->u[k]); k++; } while (k < b->nu && g && b->u[k].grp == g);
    memmove(b->u, b->u + k, (size_t)(b->nu - k) * sizeof(EdOp));
    b->nu -= k;
    if (b->clean_ok) { b->clean_at -= k; if (b->clean_at < 0) b->clean_ok = 0; }
}

static void record(EdBuf *b, int ins, int pos, const char *s, int n, int cur0, int cur1) {
    char *txt = ed_realloc(NULL, (size_t)(n ? n : 1));
    EdOp o = { ins, pos, n, txt, cur0, cur1, b->grp };
    if (!txt || push_op(&b->u, &b->nu, &b->cu, &o)) {
        ed_free(txt);
        stack_clear(&b->u, &b->nu, &b->cu); b->ubytes = 0; b->clean_ok = 0;
        return;
    }
    memcpy(txt, s, (size_t)n);
    b->ubytes += n;
    while (b->nu > MAX_OPS || (b->ubytes > MAX_UBYTES && b->nu > 1)) drop_oldest(b);
}

int eb_insert(EdBuf *b, int pos, const char *s, int n, int cur0) {
    if (n <= 0) return 0;
    if (raw_insert(b, pos, s, n)) return -1;
    clear_redo(b);
    if (!b->grp && n == 1 && b->nu > 0 && !(b->clean_ok && b->clean_at == b->nu)) {
        EdOp *p = &b->u[b->nu - 1];
        int word_break = s[0] == '\n' || (s[0] == ' ' && p->txt[p->len - 1] != ' ');
        if (p->ins && !p->grp && p->pos + p->len == pos && p->cur1 == cur0 && p->len < MAX_RUN && !word_break) {
            char *nt = ed_realloc(p->txt, (size_t)p->len + 1);
            if (nt) { p->txt = nt; p->txt[p->len++] = s[0]; p->cur1 = pos + 1; b->ubytes++; return 0; }
        }
    }
    record(b, 1, pos, s, n, cur0, pos + n);
    return 0;
}

int eb_erase(EdBuf *b, int pos, int n, int cur0) {
    if (n <= 0) return 0;
    if (pos < 0 || pos + n > b->len) return -1;
    char one[1];
    char *save = n == 1 ? one : ed_realloc(NULL, (size_t)n);
    if (!save) { raw_erase(b, pos, n); clear_redo(b); stack_clear(&b->u, &b->nu, &b->cu); b->ubytes = 0; b->clean_ok = 0; return 0; }
    memcpy(save, b->t + pos, (size_t)n);
    raw_erase(b, pos, n);
    clear_redo(b);
    if (!b->grp && n == 1 && b->nu > 0 && !(b->clean_ok && b->clean_at == b->nu)) {
        EdOp *p = &b->u[b->nu - 1];
        if (!p->ins && !p->grp && p->cur1 == cur0 && p->len < MAX_RUN) {
            if (p->pos == pos + 1) {
                char *nt = ed_realloc(p->txt, (size_t)p->len + 1);
                if (nt) { memmove(nt + 1, nt, (size_t)p->len); nt[0] = save[0]; p->txt = nt; p->len++; p->pos = pos; p->cur1 = pos; b->ubytes++; return 0; }
            } else if (p->pos == pos) {
                char *nt = ed_realloc(p->txt, (size_t)p->len + 1);
                if (nt) { nt[p->len++] = save[0]; p->txt = nt; p->cur1 = pos; b->ubytes++; return 0; }
            }
        }
    }
    record(b, 0, pos, save, n, cur0, pos);
    if (save != one) ed_free(save);
    return 0;
}

void eb_group_begin(EdBuf *b) { b->grp = ++b->grp_seq; }
void eb_group_end(EdBuf *b)   { b->grp = 0; }

void eb_mark_clean(EdBuf *b) { b->clean_at = b->nu; b->clean_ok = 1; }
int  eb_is_clean(const EdBuf *b) { return b->clean_ok && b->clean_at == b->nu; }

static int apply(EdBuf *b, const EdOp *o, int undo, int *cur) {
    int do_insert = undo ? !o->ins : o->ins;
    if (do_insert) { if (raw_insert(b, o->pos, o->txt, o->len)) return -1; }
    else raw_erase(b, o->pos, o->len);
    *cur = undo ? o->cur0 : o->cur1;
    return 0;
}

static int step(EdBuf *b, int undo, int *cur) {
    EdOp **from = undo ? &b->u : &b->r; int *nf = undo ? &b->nu : &b->nr;
    EdOp **to   = undo ? &b->r : &b->u; int *nt = undo ? &b->nr : &b->nu, *ct = undo ? &b->cr : &b->cu;
    if (!*nf) return 0;
    int g = (*from)[*nf - 1].grp, did = 0;
    do {
        EdOp o = (*from)[*nf - 1];
        if (apply(b, &o, undo, cur)) break;
        (*nf)--;
        if (push_op(to, nt, ct, &o)) op_free(&o);
        did = 1;
    } while (*nf && g && (*from)[*nf - 1].grp == g);
    b->ubytes = 0;
    for (int i = 0; i < b->nu; i++) b->ubytes += b->u[i].len;
    return did;
}

int eb_undo(EdBuf *b, int *cur) { return step(b, 1, cur); }
int eb_redo(EdBuf *b, int *cur) { return step(b, 0, cur); }

int eb_line_of(const EdBuf *b, int pos) {
    int lo = 0, hi = b->nl - 1;
    while (lo < hi) { int m = (lo + hi + 1) / 2; if (b->ls[m] <= pos) lo = m; else hi = m - 1; }
    return lo;
}
int eb_line_len(const EdBuf *b, int l) {
    int end = l + 1 < b->nl ? b->ls[l + 1] - 1 : b->len;
    return end - b->ls[l];
}

static char lc(char c, int nocase) { return nocase && c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

int eb_find(const EdBuf *b, const char *pat, int from, int dir, int nocase) {
    int m = (int)strlen(pat), total = b->len - m + 1;
    if (m == 0 || total <= 0) return -1;
    if (from < 0) from = 0;
    if (from >= total) from = dir >= 0 ? 0 : total - 1;
    for (int k = 0; k < total; k++) {
        int i = dir >= 0 ? (from + k) % total : ((from - k) % total + total) % total, j = 0;
        while (j < m && lc(b->t[i + j], nocase) == lc(pat[j], nocase)) j++;
        if (j == m) return i;
    }
    return -1;
}
