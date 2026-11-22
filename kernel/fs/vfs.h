#ifndef FS_VFS_H
#define FS_VFS_H
#include "lib/types.h"
#include "abi.h"

#define VFS_NAME    GOS_NAME_MAX
#define VFS_MAXFILE (256 * 1024)


typedef struct VNode VNode;
typedef struct { int (*read)(VNode *, u32 off, void *buf, u32 n);
                 int (*write)(VNode *, u32 off, const void *buf, u32 n); } VOps;

struct VNode {
    char name[VFS_NAME];
    u8 type, virt;
    u16 mode;
    u16 uid, gid;
    u32 size, cap, mtime, ino;
    u8 *data;
    VNode *parent, *child, *next;
    const VOps *ops;
};

typedef struct { u8 type; u16 mode; u32 size, mtime, ino; u16 uid, gid; } VStat;
typedef struct Pipe Pipe;
typedef struct File { VNode *vn; u32 off; int flags; int refs; struct Pipe *pipe; int pend; } File;

enum { P_X = 1, P_W = 2, P_R = 4 };
int    vfs_access(VNode *n, int want);
int    vfs_lookup_errno(void);
int    vfs_chown(VNode *n, u16 uid, u16 gid);
int    vfs_chmod(VNode *n, u16 mode);

void   vfs_init(void);
VNode *vfs_root(void);
VNode *vfs_lookup(VNode *cwd, const char *path);
VNode *vfs_create(VNode *dir, const char *name, int type);
int    vfs_mkdir(VNode *cwd, const char *path);
int    vfs_creat(VNode *cwd, const char *path);
int    vfs_unlink(VNode *cwd, const char *path);
int    vfs_rmdir(VNode *cwd, const char *path);
int    vfs_rename(VNode *cwd, const char *from, const char *to);
int    vfs_stat(VNode *cwd, const char *path, VStat *st);
void   vfs_node_stat(VNode *n, VStat *st);
int    vfs_node_read(VNode *n, u32 off, void *buf, u32 len);
int    vfs_node_write(VNode *n, u32 off, const void *buf, u32 len);
int    vfs_truncate(VNode *n, u32 len);
int    vfs_getpath(VNode *n, char *buf, int max);
int    vfs_join(VNode *cwd, const char *path, char *out, int max);
u32    vfs_generation(void);
u32    vfs_persist_generation(void);
int    vfs_read_file(VNode *cwd, const char *path, char **out, u32 *len);
int    vfs_write_file(VNode *cwd, const char *path, const void *data, u32 len, int append);
void   vfs_stats(u32 *files, u32 *dirs, u32 *bytes);
VNode *vfs_add_virt_file(VNode *dir, const char *name, const char *content);
VNode *vfs_add_virt_blob(VNode *dir, const char *name, const void *data, u32 size);
VNode *vfs_add_dev(VNode *dir, const char *name, const VOps *ops, void *tag);

int  fd_open(const char *path, int flags);
int  fd_read(int fd, void *buf, u32 n);
int  fd_write(int fd, const void *buf, u32 n);
int  fd_lseek(int fd, int off, int whence);
int  fd_close(int fd);
int  fd_dup(int fd);
int  fd_dup2(int oldfd, int newfd);
int  fd_pipe(int fds[2], int flags);
File *fd_dup_std(int fd);
File *file_ref(File *f);
File *fd_file(int fd);
void fd_release(File *f);

#endif
