#ifndef GOS_ERRNO_H
#define GOS_ERRNO_H
#include "lib/errno.h"
#include <stddef.h>
extern int gos_errno;
#define errno gos_errno
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP ENOTSUP
#endif
char *strerror(int errnum);
int strerror_r(int errnum, char *buf, size_t size);
void perror(const char *prefix);
#endif
