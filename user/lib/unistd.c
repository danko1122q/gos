#include "gos.h"
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <sys/stat.h>

static int cvt(int r) { if (r < 0) { errno = -r; return -1; } return r; }
ssize_t read(int fd, void *buf, size_t n) { return (ssize_t)cvt(gos_read(fd, buf, n)); }
ssize_t write(int fd, const void *buf, size_t n) { return (ssize_t)cvt(gos_write(fd, buf, n)); }
int close(int fd) { return cvt(gos_close(fd)); }
int open(const char *path, int flags, ...) {
    if (!path) { errno = EFAULT; return -1; }
    if ((flags & O_ACCMODE) == O_ACCMODE ||
        (flags & ~(O_ACCMODE | O_CREAT | O_TRUNC | O_APPEND | O_NONBLOCK))) {
        errno = EINVAL; return -1;
    }
    return cvt(gos_open(path, flags));
}
off_t lseek(int fd, off_t off, int whence) { return (off_t)cvt(gos_lseek(fd, off, whence)); }
int unlink(const char *p) { return cvt(gos_unlink(p)); }
int remove(const char *p) {
    struct stat st;
    if (stat(p, &st) < 0) return -1;
    return S_ISDIR(st.st_mode) ? rmdir(p) : unlink(p);
}
int rename(const char *a, const char *b) { return cvt(gos_rename(a, b)); }
int chdir(const char *p) { return cvt(gos_chdir(p)); }
char *getcwd(char *buf, size_t size) {
    int allocated = 0;
    if (!buf) { size = 256; buf = malloc(size); if (!buf) { errno = ENOMEM; return NULL; } allocated = 1; }
    if (!size || size > 0x7fffffffu) { if (allocated) free(buf); errno = ERANGE; return NULL; }
    int r = gos_getcwd(buf, (int)size);
    if (r < 0) { if (allocated) free(buf); errno = -r; return NULL; }
    return buf;
}
int dup(int fd) { return cvt(gos_dup(fd)); }
int dup2(int oldfd, int newfd) { return cvt(gos_dup2(oldfd, newfd)); }
int pipe2(int fds[2], int flags) {
    int f = 0;
    if (flags & O_NONBLOCK) f = PIPE_RD_NONBLOCK | PIPE_WR_NONBLOCK;
    if (flags & ~(O_NONBLOCK)) { errno = EINVAL; return -1; }
    return cvt(gos_pipe(fds, f));
}
int pipe(int fds[2]) { return cvt(gos_pipe(fds, 0)); }
pid_t getpid(void) { return gos_getpid(); }
pid_t getppid(void) {
    int pid = gos_getpid(); GProcInfo pi;
    for (int i = 0; i < GOS_MAXTASK; i++) if (gos_procinfo(i, &pi) == 0 && pi.pid == pid) return pi.ppid;
    return 0;
}
uid_t getuid(void) { return (uid_t)gos_getuid(); }
gid_t getgid(void) { int r = gos_syscall(SYS_GETGID, 0, 0, 0, 0, 0); if (r < 0) { errno = -r; return (gid_t)-1; } return (gid_t)r; }
unsigned int sleep(unsigned int sec) {
    unsigned int max = 0x7fffffffu / 1000u;
    if (sec > max) { gos_sleep_ms((int)(max * 1000u)); return sec - max; }
    gos_sleep_ms((int)(sec * 1000u));
    return 0;
}
int usleep(unsigned int usec) {
    unsigned int ms = usec / 1000u + (usec % 1000u != 0);
    if (ms > 0x7fffffffu) ms = 0x7fffffffu;
    gos_sleep_ms((int)ms); return 0;
}
int sync(void) { return cvt(gos_sync()); }
int rmdir(const char *p) { return cvt(gos_rmdir(p)); }
int mkdir(const char *p, mode_t mode) {
    int r = gos_mkdir(p);
    if (r < 0) return cvt(r);
    r = gos_chmod(p, (int)(mode & 0777u));
    return cvt(r);
}
int chmod(const char *p, mode_t mode) { return cvt(gos_chmod(p, (int)mode)); }
int chown(const char *p, uid_t uid, gid_t gid) { return cvt(gos_chown(p, uid, gid)); }
int kill(pid_t pid, int sig) {
    if (sig != 9) { errno = EINVAL; return -1; }
    return cvt(gos_kill(pid));
}
int access(const char *path, int mode) {
    GStat st;
    if (mode & ~(R_OK | W_OK | X_OK)) { errno = EINVAL; return -1; }
    int r = gos_stat(path, &st); if (r < 0) return cvt(r);
    uid_t uid = getuid(); gid_t gid = getgid();
    if (!uid) { if ((mode & X_OK) && st.type == VN_FILE && !(st.mode & 0111)) { errno = EACCES; return -1; } return 0; }
    int shift = uid == st.uid ? 6 : gid == st.gid ? 3 : 0;
    int need = ((mode & R_OK) ? 4 : 0) | ((mode & W_OK) ? 2 : 0) | ((mode & X_OK) ? 1 : 0);
    if ((((st.mode >> shift) & 7) & need) != need) { errno = EACCES; return -1; }
    return 0;
}

int isatty(int fd) {
    struct stat st;
    if (fstat(fd, &st) < 0) return 0;
    if (S_ISCHR(st.st_mode)) return 1;
    errno = ENOTTY;
    return 0;
}
