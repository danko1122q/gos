#include "gos.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

struct __gos_DIR {
    int fd;
    int index;
    char path[128];
    struct dirent entry;
};

DIR *opendir(const char *name) {
    struct stat st;
    if (!name || stat(name, &st) < 0) return NULL;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return NULL; }
    DIR *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->fd = open(name, O_RDONLY);
    if (d->fd < 0) { free(d); return NULL; }
    if (strlcpy(d->path, name, sizeof d->path) >= sizeof d->path) {
        close(d->fd); free(d); errno = ENAMETOOLONG; return NULL;
    }
    return d;
}
struct dirent *readdir(DIR *d) {
    if (!d) { errno = EBADF; return NULL; }
    if (d->index < 0 || d->index == 0x7fffffff) { errno = EOVERFLOW; return NULL; }
    GDirEnt ge;
    int r = gos_readdir(d->path, d->index, &ge);
    if (r < 0) { errno = -r; return NULL; }
    if (!r) { errno = 0; return NULL; }
    memset(&d->entry, 0, sizeof d->entry);
    d->entry.d_type = ge.type == VN_DIR ? DT_DIR : ge.type == VN_DEV ? DT_CHR : DT_REG;
    d->entry.d_off = d->index;
    d->entry.d_reclen = (u16)sizeof d->entry;
    d->entry.d_size = ge.size;
    strlcpy(d->entry.d_name, ge.name, sizeof d->entry.d_name);
    d->index++;
    return &d->entry;
}
int closedir(DIR *d) {
    if (!d) { errno = EBADF; return -1; }
    int r = close(d->fd); free(d); return r;
}
void rewinddir(DIR *d) { if (d) d->index = 0; else errno = EBADF; }
long telldir(DIR *d) { if (!d) { errno = EBADF; return -1; } return d->index; }
void seekdir(DIR *d, long pos) { if (!d || pos < 0 || pos > 0x7fffffffL) { errno = EINVAL; return; } d->index = (int)pos; }
int dirfd(DIR *d) { if (!d) { errno = EBADF; return -1; } return d->fd; }
