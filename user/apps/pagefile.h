#ifndef USER_PAGEFILE_H
#define USER_PAGEFILE_H
static inline int pf_split(const char *d, int len, int *off, int *plen, int max) {
    int n = 0, st = 0;
    for (int i = 0; i <= len; i++) {
        if (i == len || d[i] == '\f') {
            if (n < max) { off[n] = st; plen[n] = i - st; n++; }
            st = i + 1;
        }
    }
    return n;
}
#endif
