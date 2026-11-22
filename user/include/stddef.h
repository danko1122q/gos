#ifndef GOS_STDDEF_H
#define GOS_STDDEF_H
#include "lib/types.h"
typedef s32 ptrdiff_t;
#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif
typedef union { long long __ll; long double __ld; void *__p; } max_align_t;
#ifndef NULL
#define NULL ((void *)0)
#endif
#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif
#endif
