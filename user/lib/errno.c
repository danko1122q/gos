#include "gos.h"
#include <stdio.h>

int gos_errno;

char *strerror(int errnum) {
    static char unknown[32];
    const char *s;
    switch (errnum < 0 ? -errnum : errnum) {
    case 0: s = "Success"; break;
    case EPERM: s = "Operation not permitted"; break;
    case ENOENT: s = "No such file or directory"; break;
    case ESRCH: s = "No such process"; break;
    case EINTR: s = "Interrupted system call"; break;
    case EIO: s = "Input/output error"; break;
    case ENXIO: s = "No such device or address"; break;
    case E2BIG: s = "Argument list too long"; break;
    case ENOEXEC: s = "Exec format error"; break;
    case EBADF: s = "Bad file descriptor"; break;
    case ECHILD: s = "No child processes"; break;
    case EAGAIN: s = "Resource temporarily unavailable"; break;
    case ENOMEM: s = "Cannot allocate memory"; break;
    case EACCES: s = "Permission denied"; break;
    case EFAULT: s = "Bad address"; break;
    case ENOTBLK: s = "Block device required"; break;
    case EBUSY: s = "Device or resource busy"; break;
    case EEXIST: s = "File exists"; break;
    case EXDEV: s = "Invalid cross-device link"; break;
    case ENODEV: s = "No such device"; break;
    case ENOTTY: s = "Inappropriate ioctl for device"; break;
    case ETXTBSY: s = "Text file busy"; break;
    case ENOTDIR: s = "Not a directory"; break;
    case EISDIR: s = "Is a directory"; break;
    case EINVAL: s = "Invalid argument"; break;
    case EMFILE: s = "Too many open files"; break;
    case ENFILE: s = "Too many open files in system"; break;
    case ENOSPC: s = "No space left on device"; break;
    case EROFS: s = "Read-only file system"; break;
    case EMLINK: s = "Too many links"; break;
    case ESPIPE: s = "Illegal seek"; break;
    case EPIPE: s = "Broken pipe"; break;
    case ERANGE: s = "Result out of range"; break;
    case EFBIG: s = "File too large"; break;
    case ENAMETOOLONG: s = "File name too long"; break;
    case ENOSYS: s = "Function not implemented"; break;
    case ENOTEMPTY: s = "Directory not empty"; break;
    case EOVERFLOW: s = "Value too large for defined data type"; break;
    case EDOM: s = "Numerical argument out of domain"; break;
    case EDEADLK: s = "Resource deadlock avoided"; break;
    case ENOLCK: s = "No locks available"; break;
    case ELOOP: s = "Too many symbolic links"; break;
    case EILSEQ: s = "Illegal byte sequence"; break;
    case ENOTSUP: s = "Operation not supported"; break;
    default:
        ksnprintf(unknown, sizeof unknown, "Unknown error %d", errnum);
        return unknown;
    }
    return (char *)s;
}

void perror(const char *prefix) {
    if (prefix && *prefix) fprintf(stderr, "%s: %s\n", prefix, strerror(errno));
    else fprintf(stderr, "%s\n", strerror(errno));
}

void __gos_assert_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "assertion failed: %s (%s:%d)\n", expr, file, line);
    abort();
}

int strerror_r(int errnum, char *buf, size_t size) {
    if (!buf || !size) return EINVAL;
    size_t len = strlcpy(buf, strerror(errnum), size);
    return len >= size ? ERANGE : 0;
}
