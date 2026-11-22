#include "gos.h"

int gos_syscall(int n, int a, int b, int c, int d, int e) {
#if defined(__arm__)
    register int r0 __asm__("r0") = a;
    register int r1 __asm__("r1") = b;
    register int r2 __asm__("r2") = c;
    register int r3 __asm__("r3") = d;
    register int r4 __asm__("r4") = e;
    register int r7 __asm__("r7") = n;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r7) : "memory");
    return r0;
#else
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory");
    return r;
#endif
}
#define S0(n)             gos_syscall(n, 0, 0, 0, 0, 0)
#define S1(n, a)          gos_syscall(n, (int)(a), 0, 0, 0, 0)
#define S2(n, a, b)       gos_syscall(n, (int)(a), (int)(b), 0, 0, 0)
#define S3(n, a, b, c)    gos_syscall(n, (int)(a), (int)(b), (int)(c), 0, 0)

void gos_exit(int code) { for (;;) S1(SYS_EXIT, code); }
int  gos_read(int fd, void *buf, u32 n)        { return S3(SYS_READ, fd, buf, n); }
int  gos_write(int fd, const void *buf, u32 n) { return S3(SYS_WRITE, fd, buf, n); }
int  gos_open(const char *p, int flags)        { return S2(SYS_OPEN, p, flags); }
int  gos_close(int fd)                         { return S1(SYS_CLOSE, fd); }
int  gos_lseek(int fd, int off, int wh)        { return S3(SYS_LSEEK, fd, off, wh); }
int  gos_unlink(const char *p)                 { return S1(SYS_UNLINK, p); }
int  gos_mkdir(const char *p)                  { return S1(SYS_MKDIR, p); }
int  gos_rmdir(const char *p)                  { return S1(SYS_RMDIR, p); }
int  gos_rename(const char *a, const char *b)  { return S2(SYS_RENAME, a, b); }
int  gos_chmod(const char *p, int m)           { return S2(SYS_CHMOD, p, m); }
int  gos_chdir(const char *p)                  { return S1(SYS_CHDIR, p); }
int  gos_getcwd(char *b, int max)              { return S2(SYS_GETCWD, b, max); }
int  gos_stat(const char *p, GStat *st)        { return S2(SYS_STAT, p, st); }
int  gos_readdir(const char *p, int i, GDirEnt *e) { return S3(SYS_READDIR, p, i, e); }
int  gos_spawn(const char *p, const char *const *argv) { return gos_syscall(SYS_SPAWN, (int)p, (int)argv, 0, SPAWN_DETACH, 0); }
int  gos_spawn_ex(const char *p, const char *const *argv, const int *fdmap, int flags) {
    return gos_syscall(SYS_SPAWN, (int)p, (int)argv, (int)fdmap, flags, 0);
}
int  gos_wait(int pid, int *status, int flags) { return S3(SYS_WAIT, pid, status, flags); }
int  gos_pipe(int fds[2], int flags)           { return S2(SYS_PIPE, fds, flags); }
int  gos_dup(int fd)                           { return S1(SYS_DUP, fd); }
int  gos_dup2(int oldfd, int newfd)            { return S2(SYS_DUP2, oldfd, newfd); }
int  gos_getuid(void)                          { return S0(SYS_GETUID); }
int  gos_chown(const char *p, int uid, int gid) { return S3(SYS_CHOWN, p, uid, gid); }
int  gos_launch(const char *p)                 { return S1(SYS_LAUNCH, p); }
int  gos_kill(int pid)                         { return S2(SYS_KILL, pid, 9); }
int  gos_getpid(void)                          { return S0(SYS_GETPID); }
int  gos_procinfo(int slot, GProcInfo *pi)     { return S2(SYS_PROCINFO, slot, pi); }
int  gos_sysinfo(GSysInfo *si)                 { return S1(SYS_SYSINFO, si); }
int  gos_klog(char *b, int max)                { return S2(SYS_KLOG, b, max); }
int  gos_sync(void)                            { return S0(SYS_SYNC); }
void gos_reboot(void)                          { S0(SYS_REBOOT); }
void gos_poweroff(void)                        { S0(SYS_POWEROFF); }
u32  gos_time(void)                            { return (u32)S0(SYS_TIME); }
u32  gos_ticks(void)                           { return (u32)S0(SYS_TICKS); }
void gos_sleep_ms(int ms)                      { S1(SYS_SLEEPMS, ms); }
void gos_yield(void)                           { S0(SYS_YIELD); }
int  gos_brk(u32 addr)                         { return S1(SYS_BRK, addr); }
int  gos_clip_set(const void *d, u32 n)        { return S2(SYS_CLIP_SET, d, n); }
int  gos_clip_get(void *b, u32 max)            { return S2(SYS_CLIP_GET, b, max); }
int  gos_clip_text(char **out, u32 *len) {
    *out = NULL; if (len) *len = 0;
    int n = gos_clip_get(NULL, 0);
    if (n < 0) return n;
    char *b = kmalloc((size_t)n + 1);
    if (!b) return -ENOMEM;
    int g = n ? gos_clip_get(b, (u32)n) : 0;
    if (g < 0) { kfree(b); return g; }
    if (g > n) g = n;
    b[g] = 0; *out = b; if (len) *len = (u32)g;
    return 0;
}

int  gos_setting_get(int id)                   { return S1(SYS_SETTING_GET, id); }
int  gos_setting_set(int id, int v)            { return S2(SYS_SETTING_SET, id, v); }
int  gos_settime(u32 epoch)                    { return S1(SYS_SETTIME, epoch); }
void gos_beep(int hz, int ms)                  { S2(SYS_BEEP, hz, ms); }
int  gos_trash(const char *p)                  { return S3(SYS_TRASH, TRASH_MOVE, p, 0); }
int  gos_putaway(const char *p)                { return S3(SYS_TRASH, TRASH_PUTAWAY, p, 0); }
int  gos_trash_empty(void)                     { return S3(SYS_TRASH, TRASH_EMPTY, 0, 0); }
int  gos_trash_info(u32 *items, u32 *bytes) {
    u32 o[2] = { 0, 0 };
    int r = S3(SYS_TRASH, TRASH_INFO, 0, o);
    if (items) *items = o[0];
    if (bytes) *bytes = o[1];
    return r;
}
int  gos_duplicate(const char *p)              { return S1(SYS_DUPLICATE, p); }
int  gos_drag_begin(const char *p)             { return S1(SYS_DRAG_BEGIN, p); }
int  gos_drag_get(char *b, int max)            { return S2(SYS_DRAG_GET, b, max); }
int  win_confirm(const char *t, const char *l1, const char *l2, const char *ok) { return gos_syscall(SYS_CONFIRM, (int)t, (int)l1, (int)l2, (int)ok, 0); }
int  gos_dblclick_ms(void) { static const int ms[3] = { 800, 500, 300 }; int v = gos_setting_get(SET_DBLCLICK); return ms[v < 0 || v > 2 ? 1 : v]; }
