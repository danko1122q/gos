#ifndef LIB_TYPES_H
#define LIB_TYPES_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed long long   s64;
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef u32                size_t;

#define NULL ((void *)0)
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define offsetof(t, m) __builtin_offsetof(t, m)
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif
