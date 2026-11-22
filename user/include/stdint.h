#ifndef GOS_STDINT_H
#define GOS_STDINT_H
#include "lib/types.h"
typedef u8  uint8_t;
typedef u16 uint16_t;
typedef u32 uint32_t;
typedef u64 uint64_t;
typedef s8  int8_t;
typedef s16 int16_t;
typedef s32 int32_t;
typedef s64 int64_t;
typedef s64 intmax_t;
typedef u64 uintmax_t;
typedef int8_t int_least8_t; typedef uint8_t uint_least8_t;
typedef int16_t int_least16_t; typedef uint16_t uint_least16_t;
typedef int32_t int_least32_t; typedef uint32_t uint_least32_t;
typedef int64_t int_least64_t; typedef uint64_t uint_least64_t;
typedef int32_t int_fast8_t; typedef uint32_t uint_fast8_t;
typedef int32_t int_fast16_t; typedef uint32_t uint_fast16_t;
typedef int32_t int_fast32_t; typedef uint32_t uint_fast32_t;
typedef int64_t int_fast64_t; typedef uint64_t uint_fast64_t;
typedef u32 uintptr_t;
typedef s32 intptr_t;
#define UINT8_MAX  0xffu
#define UINT16_MAX 0xffffu
#define UINT32_MAX 0xffffffffu
#define INT8_MAX   0x7f
#define INT16_MAX  0x7fff
#define INT32_MAX  0x7fffffff
#define INT8_MIN   (-INT8_MAX - 1)
#define INT16_MIN  (-INT16_MAX - 1)
#define INT32_MIN  (-INT32_MAX - 1)
#define UINT64_MAX 0xffffffffffffffffULL
#define INTPTR_MAX INT32_MAX
#define INTPTR_MIN INT32_MIN
#define UINTPTR_MAX UINT32_MAX
#define INTMAX_MAX INT64_MAX
#define INT_LEAST8_MIN INT8_MIN
#define INT_LEAST8_MAX INT8_MAX
#define UINT_LEAST8_MAX UINT8_MAX
#define INT_LEAST16_MIN INT16_MIN
#define INT_LEAST16_MAX INT16_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define INT_LEAST32_MIN INT32_MIN
#define INT_LEAST32_MAX INT32_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define INT_LEAST64_MIN INT64_MIN
#define INT_LEAST64_MAX INT64_MAX
#define UINT_LEAST64_MAX UINT64_MAX
#define INT_FAST8_MIN INT32_MIN
#define INT_FAST8_MAX INT32_MAX
#define UINT_FAST8_MAX UINT32_MAX
#define INT_FAST16_MIN INT32_MIN
#define INT_FAST16_MAX INT32_MAX
#define UINT_FAST16_MAX UINT32_MAX
#define INT_FAST32_MIN INT32_MIN
#define INT_FAST32_MAX INT32_MAX
#define UINT_FAST32_MAX UINT32_MAX
#define INT_FAST64_MIN INT64_MIN
#define INT_FAST64_MAX INT64_MAX
#define UINT_FAST64_MAX UINT64_MAX
#define INTMAX_MIN INT64_MIN
#define UINTMAX_MAX UINT64_MAX
#define INT64_MAX  0x7fffffffffffffffLL
#define INT64_MIN  (-INT64_MAX - 1LL)
#define SIZE_MAX   UINT32_MAX
#define PTRDIFF_MAX INT32_MAX
#define PTRDIFF_MIN INT32_MIN
#define INT8_C(v) v
#define INT16_C(v) v
#define INT32_C(v) v
#define INT64_C(v) v##LL
#define UINT8_C(v) v##U
#define UINT16_C(v) v##U
#define UINT32_C(v) v##U
#define UINT64_C(v) v##ULL
#define INTMAX_C(v) v##LL
#define UINTMAX_C(v) v##ULL
#endif
