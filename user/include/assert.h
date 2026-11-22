#ifndef GOS_ASSERT_H
#define GOS_ASSERT_H
#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
void __gos_assert_fail(const char *expr, const char *file, int line) __attribute__((noreturn));
#define assert(expression) ((expression) ? (void)0 : __gos_assert_fail(#expression, __FILE__, __LINE__))
#endif
#endif
