#include "lib/string.h"

void *memcpy(void *d, const void *s, size_t n) {
    u8 *dp = d; const u8 *sp = s;
    while (n--) *dp++ = *sp++;
    return d;
}
void *memmove(void *d, const void *s, size_t n) {
    u8 *dp = d; const u8 *sp = s;
    if ((u32)dp <= (u32)sp) { while (n--) *dp++ = *sp++; }
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}
void *memset(void *d, int c, size_t n) {
    u8 *dp = d;
    while (n--) *dp++ = (u8)c;
    return d;
}
int memcmp(const void *a, const void *b, size_t n) {
    const u8 *x = a, *y = b;
    for (; n; n--, x++, y++) if (*x != *y) return (int)*x - (int)*y;
    return 0;
}
void *memchr(const void *s, int c, size_t n) {
    const u8 *p = s;
    while (n--) { if (*p == (u8)c) return (void *)p; p++; }
    return NULL;
}
void *memrchr(const void *s, int c, size_t n) {
    const u8 *p = (const u8 *)s + n;
    while (n--) { --p; if (*p == (u8)c) return (void *)p; }
    return NULL;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
size_t strnlen(const char *s, size_t max) { size_t n = 0; while (n < max && s[n]) n++; return n; }
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}
int strncmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) {
        if (*a != *b) return (u8)*a - (u8)*b;
        if (!*a) break;
    }
    return 0;
}
char *strncpy(char *d, const char *s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}
size_t strlcpy(char *d, const char *s, size_t n) {
    size_t l = strlen(s);
    if (n) { size_t c = l >= n ? n - 1 : l; memcpy(d, s, c); d[c] = 0; }
    return l;
}
size_t strlcat(char *d, const char *s, size_t n) {
    size_t dl = strnlen(d, n), sl = strlen(s);
    if (dl == n) return n + sl;
    size_t room = n - dl;
    if (room) { size_t c = sl >= room ? room - 1 : sl; memcpy(d + dl, s, c); d[dl + c] = 0; }
    return dl + sl;
}
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
char *strncat(char *d, const char *s, size_t n) {
    char *out = d;
    d += strlen(d);
    while (n && *s) { *d++ = *s++; n--; }
    *d = 0;
    return out;
}
char *strchr(const char *s, int c) {
    for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return NULL; }
}
char *strrchr(const char *s, int c) {
    const char *r = NULL;
    for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char *)r; }
}
char *strstr(const char *h, const char *n) {
    size_t l = strlen(n);
    if (!l) return (char *)h;
    for (; *h; h++) if (*h == *n && !strncmp(h, n, l)) return (char *)h;
    return NULL;
}
int strcoll(const char *a, const char *b) { return strcmp(a, b); }
size_t strxfrm(char *d, const char *s, size_t n) {
    size_t len = strlen(s);
    if (n) { size_t copy = len < n - 1 ? len : n - 1; memcpy(d, s, copy); d[copy] = 0; }
    return len;
}
char *strsep(char **stringp, const char *delim) {
    if (!stringp || !*stringp) return NULL;
    char *start = *stringp;
    char *end = strpbrk(start, delim);
    if (end) { *end = 0; *stringp = end + 1; }
    else *stringp = NULL;
    return start;
}
void *memccpy(void *d, const void *s, int c, size_t n) {
    u8 *out = d; const u8 *in = s;
    while (n--) { u8 v = *in++; *out++ = v; if (v == (u8)c) return out; }
    return NULL;
}
static int in_set(unsigned char c, const char *set) {
    while (*set) if ((unsigned char)*set++ == c) return 1;
    return 0;
}
size_t strspn(const char *s, const char *accept) {
    size_t n = 0; while (s[n] && in_set((u8)s[n], accept)) n++; return n;
}
size_t strcspn(const char *s, const char *reject) {
    size_t n = 0; while (s[n] && !in_set((u8)s[n], reject)) n++; return n;
}
char *strpbrk(const char *s, const char *accept) {
    while (*s) { if (in_set((u8)*s, accept)) return (char *)s; s++; } return NULL;
}
char *strtok_r(char *s, const char *delim, char **saveptr) {
    if (!s) s = *saveptr;
    if (!s) return NULL;
    s += strspn(s, delim);
    if (!*s) { *saveptr = NULL; return NULL; }
    char *start = s;
    s += strcspn(s, delim);
    if (*s) *s++ = 0;
    *saveptr = s;
    return start;
}
char *strtok(char *s, const char *delim) {
    static char *save;
    return strtok_r(s, delim, &save);
}
int strcasecmp(const char *a, const char *b) {
    while (*a && tolower((u8)*a) == tolower((u8)*b)) { a++; b++; }
    return tolower((u8)*a) - tolower((u8)*b);
}
int strncasecmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) {
        int x = tolower((u8)*a), y = tolower((u8)*b);
        if (x != y || !x) return x - y;
    }
    return 0;
}
char *strcasestr(const char *h, const char *n) {
    size_t len = strlen(n);
    if (!len) return (char *)h;
    for (; *h; h++) if (tolower((u8)*h) == tolower((u8)*n) && !strncasecmp(h, n, len)) return (char *)h;
    return NULL;
}

int atoi(const char *s) {
    unsigned int v = 0; int neg = 0;
    while (isspace((u8)*s)) s++;
    if (*s == '+' || *s == '-') { neg = *s == '-'; s++; }
    while (isdigit((u8)*s)) {
        unsigned int d = (unsigned int)(*s++ - '0');
        if (v > 214748364u || (v == 214748364u && d > (unsigned int)(neg ? 8 : 7)))
            return neg ? (-2147483647 - 1) : 2147483647;
        v = v * 10u + d;
    }
    return neg ? (v == 2147483648u ? (-2147483647 - 1) : -(int)v) : (int)v;
}

int isascii(int c) { return (unsigned int)c <= 0x7f; }
int isdigit(int c) { return (unsigned int)c - '0' <= 9; }
int islower(int c) { return (unsigned int)c - 'a' <= 25; }
int isupper(int c) { return (unsigned int)c - 'A' <= 25; }
int isalpha(int c) { return islower(c) || isupper(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || (unsigned int)c - '\t' <= ('\r' - '\t'); }
int isblank(int c) { return c == ' ' || c == '\t'; }
int iscntrl(int c) { return (unsigned int)c < 0x20 || c == 0x7f; }
int isprint(int c) { return (unsigned int)c >= 0x20 && (unsigned int)c <= 0x7e; }
int isgraph(int c) { return isprint(c) && c != ' '; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int isxdigit(int c) { return isdigit(c) || ((unsigned int)tolower(c) - 'a' <= 5); }
int tolower(int c) { return isupper(c) ? c + ('a' - 'A') : c; }
int toupper(int c) { return islower(c) ? c - ('a' - 'A') : c; }
