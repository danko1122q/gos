#include "gos.h"
#include <sys/stat.h>

static void convert_stat(const GStat *in, struct stat *out) {
    memset(out, 0, sizeof *out);
    out->st_ino = in->ino;
    out->st_mode = in->mode;
    if (in->type == VN_DIR) out->st_mode |= S_IFDIR;
    else if (in->type == VN_DEV) out->st_mode |= S_IFCHR;
    else if (in->type == VN_PIPE) out->st_mode |= S_IFIFO;
    else out->st_mode |= S_IFREG;
    out->st_nlink = 1;
    out->st_uid = in->uid; out->st_gid = in->gid;
    out->st_size = (s32)in->size;
    out->st_blksize = 512;
    out->st_blocks = (s32)((in->size + 511u) / 512u);
    out->st_atime = out->st_mtime = out->st_ctime = in->mtime;
}
static int cvt_stat(int r) { if (r < 0) { errno = -r; return -1; } return r; }
int stat(const char *path, struct stat *st) {
    GStat gs;
    if (!st) { errno = EFAULT; return -1; }
    int r = gos_stat(path, &gs); if (r < 0) return cvt_stat(r);
    convert_stat(&gs, st); return 0;
}
int lstat(const char *path, struct stat *st) { return stat(path, st); }
int fstat(int fd, struct stat *st) {
    GStat gs;
    if (!st) { errno = EFAULT; return -1; }
    int r = gos_syscall(SYS_FSTAT, fd, (int)&gs, 0, 0, 0);
    if (r < 0) return cvt_stat(r);
    convert_stat(&gs, st); return 0;
}
