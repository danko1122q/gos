#ifndef LIB_STRING_H
#define LIB_STRING_H
#include "lib/types.h"

void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
void  *memset(void *d, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t max);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strncpy(char *d, const char *s, size_t n);
size_t strlcpy(char *d, const char *s, size_t n);
char  *strcpy(char *d, const char *s);
char  *strcat(char *d, const char *s);
char  *strncat(char *d, const char *s, size_t n);
size_t strlcat(char *d, const char *s, size_t n);
char  *strsep(char **stringp, const char *delim);
int    strcoll(const char *a, const char *b);
size_t strxfrm(char *d, const char *s, size_t n);
void  *memchr(const void *s, int c, size_t n);
void  *memrchr(const void *s, int c, size_t n);
void  *memccpy(void *d, const void *s, int c, size_t n);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *h, const char *n);
char  *strtok(char *s, const char *delim);
char  *strtok_r(char *s, const char *delim, char **saveptr);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char  *strpbrk(const char *s, const char *accept);
int    strcasecmp(const char *a, const char *b);
int    strncasecmp(const char *a, const char *b, size_t n);
char  *strcasestr(const char *h, const char *n);
int    atoi(const char *s);
int    isalpha(int c);
int    isalnum(int c);
int    isascii(int c);
int    isblank(int c);
int    iscntrl(int c);
int    isdigit(int c);
int    isgraph(int c);
int    islower(int c);
int    isprint(int c);
int    ispunct(int c);
int    isspace(int c);
int    isupper(int c);
int    isxdigit(int c);
int    tolower(int c);
int    toupper(int c);

#endif
