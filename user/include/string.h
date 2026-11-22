#ifndef GOS_USER_STRING_H
#define GOS_USER_STRING_H
#include "lib/string.h"
#include "lib/types.h"
size_t strnlen(const char *s, size_t maxlen);
char *strdup(const char *s);
char *strndup(const char *s, size_t n);
char *strncat(char *dst, const char *src, size_t n);
char *strtok(char *s, const char *delim);
char *strtok_r(char *s, const char *delim, char **saveptr);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *s, const char *accept);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
char *strcasestr(const char *haystack, const char *needle);
int strcoll(const char *a, const char *b);
size_t strxfrm(char *dst, const char *src, size_t n);
char *strsep(char **stringp, const char *delim);
void *memccpy(void *dst, const void *src, int c, size_t n);
size_t strlcat(char *dst, const char *src, size_t size);
void *memchr(const void *s, int c, size_t n);
void *memrchr(const void *s, int c, size_t n);
#endif
