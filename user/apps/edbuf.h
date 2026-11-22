#ifndef USER_EDBUF_H
#define USER_EDBUF_H

typedef struct { int ins, pos, len; char *txt; int cur0, cur1, grp; } EdOp;

typedef struct EdBuf {
    char *t; int len, cap, max;
    int nl; int *ls;
    EdOp *u; int nu, cu;
    EdOp *r; int nr, cr;
    int ubytes;
    int grp, grp_seq;
    int clean_at, clean_ok;
} EdBuf;

void eb_init(EdBuf *b, int maxlen);
void eb_free(EdBuf *b);
int  eb_load(EdBuf *b, const char *s, int n);
int  eb_insert(EdBuf *b, int pos, const char *s, int n, int cur0);
int  eb_erase(EdBuf *b, int pos, int n, int cur0);
int  eb_undo(EdBuf *b, int *cur);
int  eb_redo(EdBuf *b, int *cur);
void eb_group_begin(EdBuf *b);
void eb_group_end(EdBuf *b);
void eb_mark_clean(EdBuf *b);
int  eb_is_clean(const EdBuf *b);
int  eb_line_of(const EdBuf *b, int pos);
int  eb_line_len(const EdBuf *b, int line);
int  eb_find(const EdBuf *b, const char *pat, int from, int dir, int nocase);

#endif
