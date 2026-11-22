#ifndef GOS_H
#define GOS_H
#include "abi.h"
#include "lib/types.h"
#include "lib/errno.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/timeconv.h"
#include "lib/buf.h"
#include "mem/heap.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/icons.h"
#include "gui/metrics.h"
#include "gui/widget.h"

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <getopt.h>

#define GOS_LIBC_VERSION "0.6.0"

int  gos_syscall(int n, int a, int b, int c, int d, int e);
void gos_exit(int code) __attribute__((noreturn));
int  gos_read(int fd, void *buf, u32 n);
int  gos_write(int fd, const void *buf, u32 n);
int  gos_open(const char *path, int flags);
int  gos_close(int fd);
int  gos_lseek(int fd, int off, int whence);
int  gos_unlink(const char *path);
int  gos_mkdir(const char *path);
int  gos_rmdir(const char *path);
int  gos_rename(const char *from, const char *to);
int  gos_chmod(const char *path, int mode);
int  gos_chdir(const char *path);
int  gos_getcwd(char *buf, int max);
int  gos_stat(const char *path, GStat *st);
int  gos_readdir(const char *path, int idx, GDirEnt *e);
int  gos_spawn(const char *path, const char *const *argv);
int  gos_spawn_ex(const char *path, const char *const *argv, const int *fdmap, int flags);
int  gos_wait(int pid, int *status, int flags);
int  gos_pipe(int fds[2], int flags);
int  gos_dup(int fd);
int  gos_dup2(int oldfd, int newfd);
int  gos_getuid(void);
int  gos_chown(const char *path, int uid, int gid);
int  gos_launch(const char *path);
int  gos_kill(int pid);
int  gos_getpid(void);
int  gos_procinfo(int slot, GProcInfo *pi);
int  gos_sysinfo(GSysInfo *si);
int  gos_klog(char *buf, int max);
int  gos_sync(void);
void gos_reboot(void);
void gos_poweroff(void);
u32  gos_time(void);
u32  gos_ticks(void);
void gos_sleep_ms(int ms);
void gos_yield(void);
int  gos_brk(u32 addr);
int  gos_clip_set(const void *data, u32 n);
int  gos_clip_get(void *buf, u32 max);
int  gos_clip_text(char **out, u32 *len);

int  gos_setting_get(int id);
int  gos_setting_set(int id, int v);
int  gos_settime(u32 epoch);
void gos_beep(int hz, int ms);
int  gos_trash(const char *path);
int  gos_putaway(const char *path);
int  gos_trash_empty(void);
int  gos_trash_info(u32 *items, u32 *bytes);
int  gos_duplicate(const char *path);
int  gos_drag_begin(const char *path);
int  gos_drag_get(char *buf, int max);
int  win_confirm(const char *title, const char *l1, const char *l2, const char *ok);
int  gos_dblclick_ms(void);

int  gfs_read(const char *path, char **out, u32 *len);
int  gfs_write(const char *path, const void *data, u32 len, int append);
int  gfs_isdir(const char *path);
int  gfs_exists(const char *path);
int  path_join(const char *path, char *out, int max);
const char *gos_strerror(int e);
void gos_dprintf(int fd, const char *fmt, ...);

typedef struct { int id; u8 *surf; int cw, ch; } Win;
int  win_create(Win *w, const char *title, int x, int y, int ww, int wh);
void win_begin(Win *w);
int  win_next(Win *w, GEvent *e, int timeout_ms);
void win_present(Win *w);
void win_set_title(Win *w, const char *s);
void win_set_info(Win *w, const char *s);
void win_set_scroll(Win *w, int pos, int total, int view);
void win_set_menu(Win *w, const char *files, const char *views);
void win_set_edit_menu(Win *w, const char *items);
void win_set_minsize(Win *w, int minw, int minh);
void win_close(Win *w);
void win_msgbox(const char *title, const char *l1, const char *l2);

#endif
