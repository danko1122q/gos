#include "gos.h"

int gfs_read(const char *path, char **out, u32 *len) {
    int fd = gos_open(path, O_RDONLY);
    if (fd < 0) return fd;
    GStat st; u32 cap = 4096;
    if (gos_stat(path, &st) == 0 && st.type == VN_FILE) cap = st.size + 1;
    char *b = kmalloc(cap + 1);
    if (!b) { gos_close(fd); return -ENOMEM; }
    u32 n = 0; int r;
    for (;;) {
        if (n + 1 >= cap) {
            if (cap > (1u << 22)) { r = -ENOSPC; break; }
            cap *= 2; char *nb = krealloc(b, cap + 1);
            if (!nb) { r = -ENOMEM; break; }
            b = nb;
        }
        r = gos_read(fd, b + n, cap - n);
        if (r <= 0) break;
        n += (u32)r;
    }
    gos_close(fd);
    if (r < 0) { kfree(b); return r; }
    b[n] = 0; *out = b; if (len) *len = n;
    return 0;
}

int gfs_write(const char *path, const void *data, u32 len, int append) {
    int fd = gos_open(path, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC));
    if (fd < 0) return fd;
    int r = len ? gos_write(fd, data, len) : 0;
    gos_close(fd);
    return r < 0 ? r : 0;
}

int gfs_exists(const char *path) { GStat st; return gos_stat(path, &st) == 0; }
int gfs_isdir(const char *path)  { GStat st; return gos_stat(path, &st) == 0 && st.type == VN_DIR; }

int path_join(const char *path, char *out, int max) {
    char full[512], cwd[256];
    full[0] = 0;
    if (path[0] != '/') { if (gos_getcwd(cwd, sizeof cwd) < 0) strcpy(cwd, "/"); strlcpy(full, cwd, sizeof full); }
    strlcpy(full + strlen(full), "/", sizeof full - strlen(full));
    strlcpy(full + strlen(full), path, sizeof full - strlen(full));
    char comp[32][GOS_NAME_MAX]; int nc = 0;
    const char *p = full;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        char c[GOS_NAME_MAX]; int i = 0;
        while (*p && *p != '/') { if (i < GOS_NAME_MAX - 1) c[i++] = *p; p++; }
        c[i] = 0;
        if (!strcmp(c, ".")) continue;
        if (!strcmp(c, "..")) { if (nc) nc--; continue; }
        if (nc < 32) strlcpy(comp[nc++], c, GOS_NAME_MAX);
    }
    out[0] = 0;
    if (!nc) { strlcpy(out, "/", max); return 0; }
    for (int i = 0; i < nc; i++) {
        strlcpy(out + strlen(out), "/", max - (int)strlen(out));
        strlcpy(out + strlen(out), comp[i], max - (int)strlen(out));
    }
    return 0;
}

const char *gos_strerror(int e) {
    switch (e < 0 ? -e : e) {
    case ENOENT: return "No such file or directory";   case EEXIST: return "File exists";
    case ENOTDIR: return "Not a directory";            case EISDIR: return "Is a directory";
    case EINVAL: return "Invalid argument";            case ENOSPC: return "No space left (file too large)";   case EFBIG: return "File too large";
    case ENOTEMPTY: return "Directory not empty";      case EACCES: return "Permission denied";
    case ENOMEM: return "Out of memory";               case ESRCH: return "No such process";
    case EPERM: return "Operation not permitted";      case EIO: return "I/O error";
    case EBUSY: return "Resource busy (in use by another process)";
    case EAGAIN: return "Resource temporarily unavailable (process table full?)";          case ENOEXEC: return "Exec format error";
    case EFAULT: return "Bad address";                 case ENAMETOOLONG: return "File name too long";
    case EMFILE: return "Too many open files";         case ECHILD: return "No child processes";
    case EPIPE: return "Broken pipe";                  case EBADF: return "Bad file descriptor";
    case ESPIPE: return "Illegal seek";                case ERANGE: return "Result too large";
    case E2BIG: return "Argument list too long";
    default: return "Error";
    }
}

void gos_dprintf(int fd, const char *fmt, ...) {
    char b[256];
    va_list ap; va_start(ap, fmt);
    int n = kvsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof b) n = sizeof b - 1;
    gos_write(fd, b, (u32)n);
}

int kprintf(const char *fmt, ...) {
    char b[256];
    va_list ap; va_start(ap, fmt);
    int n = kvsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof b) n = sizeof b - 1;
    gos_write(1, b, (u32)n);
    return n;
}
void kputs(const char *s) { gos_write(1, s, (u32)strlen(s)); }
