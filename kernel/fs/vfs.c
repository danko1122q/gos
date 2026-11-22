#include "fs/vfs.h"
#include "fs/devfs.h"
#include "fs/pipe.h"
#include "mem/heap.h"
#include "lib/string.h"
#include "lib/errno.h"
#include "proc/task.h"
#include "drivers/rtc.h"

static VNode *root;
static u32 gen, pgen, next_ino = 1;
static int last_err = -ENOENT;

int vfs_lookup_errno(void) { return last_err; }

u32 vfs_generation(void) { return gen; }
u32 vfs_persist_generation(void) { return pgen; }
VNode *vfs_root(void) { return root; }
static void touch(VNode *n, int persist) {
    gen++; if (persist && !n->virt) pgen++;
    n->mtime = time_now();
}

int vfs_access(VNode *n, int want) {
    Task *t = task_current();
    u16 uid = t ? t->uid : 0, gid = t ? t->gid : 0;
    if (uid == 0) {
        if ((want & P_X) && n->type == VN_FILE && !(n->mode & 0111)) return -EACCES;
        return 0;
    }
    int sh = uid == n->uid ? 6 : gid == n->gid ? 3 : 0;
    return (((n->mode >> sh) & 7) & want) == want ? 0 : -EACCES;
}

static int sticky_ok(VNode *dir, VNode *n) {
    if (!(dir->mode & 01000)) return 0;
    Task *t = task_current();
    u16 uid = t ? t->uid : 0;
    return (uid == 0 || uid == n->uid || uid == dir->uid) ? 0 : -EACCES;
}

int vfs_chown(VNode *n, u16 uid, u16 gid) {
    Task *t = task_current();
    if (t && t->uid != 0) return -EPERM;
    if (n->virt) return -EACCES;
    n->uid = uid; n->gid = gid;
    touch(n, 1); pgen++;
    return 0;
}

int vfs_chmod(VNode *n, u16 mode) {
    Task *t = task_current();
    if (n->virt) return -EACCES;
    if (t && t->uid != 0 && t->uid != n->uid) return -EPERM;
    n->mode = mode & 01777;
    touch(n, 1); pgen++;
    return 0;
}

static VNode *node_new(const char *name, int type) {
    VNode *n = kzalloc(sizeof(VNode));
    if (!n) return NULL;
    strlcpy(n->name, name, VFS_NAME);
    n->type = type; n->mode = type == VN_DIR ? 0755 : 0644;
    n->ino = next_ino++; n->mtime = time_now();
    Task *t = task_current();
    if (t) { n->uid = t->uid; n->gid = t->gid; }
    return n;
}

static void link_child(VNode *dir, VNode *n) {
    n->parent = dir;
    VNode **pp = &dir->child;
    while (*pp && strcmp((*pp)->name, n->name) < 0) pp = &(*pp)->next;
    n->next = *pp; *pp = n;
}
static void unlink_child(VNode *n) {
    VNode **pp = &n->parent->child;
    while (*pp && *pp != n) pp = &(*pp)->next;
    if (*pp) *pp = n->next;
    n->next = NULL;
}

static VNode *find_child(VNode *dir, const char *name) {
    for (VNode *c = dir->child; c; c = c->next) if (!strcmp(c->name, name)) return c;
    return NULL;
}

static int name_ok(const char *s) {
    int l = (int)strlen(s);
    return l > 0 && l < VFS_NAME && !strchr(s, '/');
}

static int next_comp(const char **p, char *out) {
    while (**p == '/') (*p)++;
    if (!**p) return 0;
    int i = 0;
    while (**p && **p != '/') { if (i < VFS_NAME) out[i++] = **p; (*p)++; }
    out[i < VFS_NAME ? i : VFS_NAME - 1] = 0;
    return 1;
}

static VNode *walk(VNode *cwd, const char *path, int stop_before_last, char *last) {
    VNode *n = (path[0] == '/' || !cwd) ? root : cwd;
    const char *p = path;
    char c[VFS_NAME];
    last_err = -ENOENT;
    while (next_comp(&p, c)) {
        const char *q = p; char peek[VFS_NAME];
        int more = next_comp(&q, peek);
        if (stop_before_last && !more) { if (last) strlcpy(last, c, VFS_NAME); return n; }
        if (n->type != VN_DIR) { last_err = -ENOTDIR; return NULL; }
        if (vfs_access(n, P_X)) { last_err = -EACCES; return NULL; }
        if (!strcmp(c, ".")) continue;
        if (!strcmp(c, "..")) { if (n->parent) n = n->parent; continue; }
        VNode *k = find_child(n, c);
        if (!k) return NULL;
        n = k;
    }
    if (stop_before_last) { if (last) last[0] = 0; return n; }
    return n;
}

VNode *vfs_lookup(VNode *cwd, const char *path) {
    if (!path || !*path) return cwd ? cwd : root;
    return walk(cwd, path, 0, NULL);
}

int vfs_getpath(VNode *n, char *buf, int max) {
    char tmp[256]; int pos = sizeof tmp - 1; tmp[pos] = 0;
    if (!n->parent) { strlcpy(buf, "/", max); return 1; }
    for (; n->parent; n = n->parent) {
        int l = (int)strlen(n->name);
        if (pos - l - 1 < 0) break;
        pos -= l; memcpy(tmp + pos, n->name, l);
        tmp[--pos] = '/';
    }
    strlcpy(buf, tmp + pos, max);
    return (int)strlen(buf);
}

int vfs_join(VNode *cwd, const char *path, char *out, int max) {
    char base[256], full[512];
    if (path[0] == '/') base[0] = 0; else vfs_getpath(cwd ? cwd : root, base, sizeof base);
    strlcpy(full, base, sizeof full);
    strlcpy(full + strlen(full), "/", sizeof full - strlen(full));
    strlcpy(full + strlen(full), path, sizeof full - strlen(full));
    char comp[32][VFS_NAME]; int nc = 0;
    const char *p = full; char c[VFS_NAME];
    while (next_comp(&p, c)) {
        if (!strcmp(c, ".")) continue;
        if (!strcmp(c, "..")) { if (nc) nc--; continue; }
        if (nc < 32) strlcpy(comp[nc++], c, VFS_NAME);
    }
    out[0] = 0;
    if (!nc) { strlcpy(out, "/", max); return 0; }
    for (int i = 0; i < nc; i++) {
        strlcpy(out + strlen(out), "/", max - strlen(out));
        strlcpy(out + strlen(out), comp[i], max - strlen(out));
    }
    return 0;
}

VNode *vfs_create(VNode *dir, const char *name, int type) {
    if (!dir || dir->type != VN_DIR || !name_ok(name) || find_child(dir, name)) return NULL;
    preempt_disable();
    VNode *n = node_new(name, type);
    if (n) { n->virt = dir->virt; link_child(dir, n); touch(dir, 1); gen++; }
    preempt_enable();
    return n;
}

static int create_at(VNode *cwd, const char *path, int type) {
    char last[VFS_NAME];
    VNode *dir = walk(cwd, path, 1, last);
    if (!dir) return last_err;
    if (dir->type != VN_DIR) return -ENOTDIR;
    if (!last[0] || !strcmp(last, ".") || !strcmp(last, "..")) return -EEXIST;
    if (find_child(dir, last)) return -EEXIST;
    if (!name_ok(last)) return -EINVAL;
    int a = vfs_access(dir, P_W | P_X);
    if (a) return a;
    return vfs_create(dir, last, type) ? 0 : -ENOMEM;
}
int vfs_mkdir(VNode *cwd, const char *path) { return create_at(cwd, path, VN_DIR); }
int vfs_creat(VNode *cwd, const char *path) { return create_at(cwd, path, VN_FILE); }

static void node_free(VNode *n) { if (n->data && !n->virt) kfree(n->data); kfree(n); }

int vfs_unlink(VNode *cwd, const char *path) {
    VNode *n = vfs_lookup(cwd, path);
    if (!n) return last_err;
    if (n->type == VN_DIR) return -EISDIR;
    if (n->virt) return -EACCES;
    int a = n->parent ? (vfs_access(n->parent, P_W | P_X) ?: sticky_ok(n->parent, n)) : -EACCES;
    if (a) return a;
    if (task_cwd_in_use(n)) return -EBUSY;
    preempt_disable();
    VNode *par = n->parent;
    unlink_child(n); touch(par, 1); pgen++; node_free(n);
    preempt_enable();
    return 0;
}

int vfs_rmdir(VNode *cwd, const char *path) {
    VNode *n = vfs_lookup(cwd, path);
    if (!n) return last_err;
    if (n->type != VN_DIR) return -ENOTDIR;
    if (n->child) return -ENOTEMPTY;
    if (!n->parent || n->virt) return -EACCES;
    int a = vfs_access(n->parent, P_W | P_X) ?: sticky_ok(n->parent, n);
    if (a) return a;
    if (task_cwd_in_use(n)) return -EBUSY;
    preempt_disable();
    VNode *par = n->parent;
    unlink_child(n); touch(par, 1); pgen++; node_free(n);
    preempt_enable();
    return 0;
}

int vfs_rename(VNode *cwd, const char *from, const char *to) {
    VNode *n = vfs_lookup(cwd, from);
    if (!n) return last_err;
    if (!n->parent || n->virt) return -EACCES;
    char last[VFS_NAME];
    VNode *dir = walk(cwd, to, 1, last);
    if (!dir) return last_err;
    if (dir->type != VN_DIR) return -ENOENT;
    int pa = vfs_access(n->parent, P_W | P_X) ?: sticky_ok(n->parent, n);
    if (!pa) pa = vfs_access(dir, P_W | P_X);
    if (pa) return pa;
    if (!last[0]) strlcpy(last, n->name, VFS_NAME);
    if (!name_ok(last)) return -EINVAL;
    for (VNode *a = dir; a; a = a->parent) if (a == n) return -EINVAL;
    VNode *ex = find_child(dir, last);
    if (ex == n) return 0;
    if (ex) {
        if (ex->type == VN_DIR) return -EEXIST;
        if (ex->virt) return -EACCES;
        int sa = sticky_ok(dir, ex); if (sa) return sa;
        if (task_cwd_in_use(ex)) return -EBUSY;
    }
    preempt_disable();
    if (ex) { unlink_child(ex); node_free(ex); }
    VNode *old = n->parent;
    unlink_child(n);
    strlcpy(n->name, last, VFS_NAME);
    link_child(dir, n);
    touch(old, 1); touch(dir, 1); pgen++;
    preempt_enable();
    return 0;
}

void vfs_node_stat(VNode *n, VStat *st) {
    st->type = n->type; st->mode = n->mode; st->size = n->size; st->mtime = n->mtime; st->ino = n->ino;
    st->uid = n->uid; st->gid = n->gid;
}
int vfs_stat(VNode *cwd, const char *path, VStat *st) {
    VNode *n = vfs_lookup(cwd, path);
    if (!n) return last_err;
    vfs_node_stat(n, st);
    return 0;
}

int vfs_node_read(VNode *n, u32 off, void *buf, u32 len) {
    if (n->type == VN_DIR) return -EISDIR;
    if (n->type == VN_DEV) return n->ops && n->ops->read ? n->ops->read(n, off, buf, len) : -EINVAL;
    preempt_disable();
    int r = 0;
    if (off < n->size) { r = (int)MIN(len, n->size - off); memcpy(buf, n->data + off, r); }
    preempt_enable();
    return r;
}

static int grow(VNode *n, u32 need) {
    if (need <= n->cap) return 0;
    if (need > VFS_MAXFILE) return -ENOSPC;
    u32 nc = n->cap ? n->cap : 64;
    while (nc < need) nc *= 2;
    u8 *nd = krealloc(n->data, nc);
    if (!nd) return -ENOMEM;
    memset(nd + n->cap, 0, nc - n->cap);
    n->data = nd; n->cap = nc;
    return 0;
}

int vfs_node_write(VNode *n, u32 off, const void *buf, u32 len) {
    if (n->type == VN_DIR) return -EISDIR;
    if (n->type == VN_DEV) return n->ops && n->ops->write ? n->ops->write(n, off, buf, len) : -EINVAL;
    if (n->virt) return -EACCES;
    preempt_disable();
    int r = grow(n, off + len);
    if (!r) {
        if (off > n->size) memset(n->data + n->size, 0, off - n->size);
        memcpy(n->data + off, buf, len);
        if (off + len > n->size) n->size = off + len;
        touch(n, 1);
        r = (int)len;
    }
    preempt_enable();
    return r;
}

int vfs_truncate(VNode *n, u32 len) {
    if (n->type != VN_FILE || n->virt) return -EACCES;
    preempt_disable();
    int r = grow(n, len);
    if (!r) { if (len > n->size) memset(n->data + n->size, 0, len - n->size); n->size = len; touch(n, 1); }
    preempt_enable();
    return r;
}

int vfs_read_file(VNode *cwd, const char *path, char **out, u32 *len) {
    VNode *n = vfs_lookup(cwd, path);
    if (!n) return -ENOENT;
    if (n->type == VN_DIR) return -EISDIR;
    u32 cap = n->type == VN_FILE ? n->size : 8192;
    char *b = kmalloc(cap + 1);
    if (!b) return -ENOMEM;
    int r = vfs_node_read(n, 0, b, cap);
    if (r < 0) { kfree(b); return r; }
    b[r] = 0; *out = b; if (len) *len = r;
    return 0;
}

int vfs_write_file(VNode *cwd, const char *path, const void *data, u32 len, int append) {
    VNode *n = vfs_lookup(cwd, path);
    if (!n) { int r = vfs_creat(cwd, path); if (r) return r; n = vfs_lookup(cwd, path); }
    if (!n) return -ENOENT;
    if (n->type == VN_DIR) return -EISDIR;
    if (n->type == VN_FILE && !append) { int r = vfs_truncate(n, 0); if (r) return r; }
    int r = vfs_node_write(n, append ? n->size : 0, data, len);
    if (r == 0 && len == 0) { touch(n, 1); }
    return r < 0 ? r : 0;
}

VNode *vfs_add_virt_file(VNode *dir, const char *name, const char *content) {
    VNode *n = vfs_create(dir, name, VN_FILE);
    if (!n) return NULL;
    n->virt = 1; n->mode = 0755;
    n->data = (u8 *)content; n->size = strlen(content);
    return n;
}
VNode *vfs_add_virt_blob(VNode *dir, const char *name, const void *data, u32 size) {
    VNode *n = vfs_create(dir, name, VN_FILE);
    if (!n) return NULL;
    n->virt = 1; n->mode = 0755;
    n->data = (u8 *)data; n->size = size;
    return n;
}
VNode *vfs_add_dev(VNode *dir, const char *name, const VOps *ops, void *tag) {
    VNode *n = vfs_create(dir, name, VN_DEV);
    if (!n) return NULL;
    n->virt = 1; n->ops = ops; n->data = tag; n->mode = 0666;
    return n;
}

static void count(VNode *n, u32 *f, u32 *d, u32 *b) {
    for (VNode *c = n->child; c; c = c->next) {
        if (c->virt) continue;
        if (c->type == VN_DIR) { (*d)++; count(c, f, d, b); } else { (*f)++; *b += c->size; }
    }
}
void vfs_stats(u32 *files, u32 *dirs, u32 *bytes) {
    *files = *dirs = *bytes = 0; count(root, files, dirs, bytes);
}

void vfs_init(void) {
    root = node_new("", VN_DIR);
    root->virt = 0;
    const char *virt_dirs[] = { "dev", "proc", "bin" };
    for (int i = 0; i < 3; i++) { VNode *d = vfs_create(root, virt_dirs[i], VN_DIR); d->virt = 1; }
    devfs_init(vfs_lookup(NULL, "/dev"));
    procfs_init(vfs_lookup(NULL, "/proc"));
    gen++; pgen = 0;
}

static File *file_alloc(VNode *vn, int flags) {
    File *f = kzalloc(sizeof(File));
    if (f) { f->vn = vn; f->flags = flags; f->refs = 1; }
    return f;
}
File *file_ref(File *f) { if (f) f->refs++; return f; }
void fd_release(File *f) {
    if (!f || --f->refs > 0) return;
    if (f->pipe) pipe_close_end(f->pipe, f->pend);
    kfree(f);
}

File *fd_dup_std(int fd) {
    Task *k = task_get(0);
    if (!k || !k->fds[fd]) return NULL;
    return file_ref(k->fds[fd]);
}

static File *getf(int fd) {
    Task *t = task_current();
    if (fd < 0 || fd >= MAXFD || !t || !t->fds[fd]) return NULL;
    return t->fds[fd];
}
File *fd_file(int fd) { return getf(fd); }

static int fd_free_slot(Task *t) {
    for (int i = 0; i < MAXFD; i++) if (!t->fds[i]) return i;
    return -1;
}

int fd_open(const char *path, int flags) {
    Task *t = task_current();
    int fd = fd_free_slot(t);
    if (fd < 0) return -EMFILE;
    int acc = flags & 3;
    VNode *n = vfs_lookup(t->cwd, path);
    if (!n) {
        if (last_err == -EACCES) return -EACCES;
        if (!(flags & O_CREAT)) return -ENOENT;
        int r = vfs_creat(t->cwd, path);
        if (r) return r;
        n = vfs_lookup(t->cwd, path);
        if (!n) return -ENOENT;
    } else {
        int need = (acc == O_RDONLY ? P_R : acc == O_WRONLY ? P_W : P_R | P_W);
        if ((flags & O_TRUNC) && n->type == VN_FILE) need |= P_W;
        if (n->type == VN_DIR && acc != O_RDONLY) return -EISDIR;
        int a = vfs_access(n, need);
        if (a) return a;
    }
    if ((flags & O_TRUNC) && n->type == VN_FILE) { int r = vfs_truncate(n, 0); if (r) return r; }
    File *f = file_alloc(n, flags);
    if (!f) return -ENOMEM;
    t->fds[fd] = f;
    return fd;
}

int fd_read(int fd, void *buf, u32 n) {
    File *f = getf(fd);
    if (!f) return -EBADF;
    if (f->pipe) return f->pend == 0 ? pipe_read(f->pipe, buf, n, f->flags & O_NONBLOCK) : -EBADF;
    if ((f->flags & 3) == O_WRONLY) return -EBADF;
    preempt_disable();
    int r = vfs_node_read(f->vn, f->off, buf, n);
    if (r > 0) f->off += r;
    preempt_enable();
    return r;
}
int fd_write(int fd, const void *buf, u32 n) {
    File *f = getf(fd);
    if (!f) return -EBADF;
    if (f->pipe) return f->pend == 1 ? pipe_write(f->pipe, buf, n, f->flags & O_NONBLOCK) : -EBADF;
    if ((f->flags & 3) == O_RDONLY) return -EBADF;
    preempt_disable();
    if (f->flags & O_APPEND) f->off = f->vn->size;
    int r = vfs_node_write(f->vn, f->off, buf, n);
    if (r > 0) f->off += r;
    preempt_enable();
    return r;
}
int fd_lseek(int fd, int off, int whence) {
    File *f = getf(fd);
    if (!f) return -EBADF;
    if (f->pipe) return -ESPIPE;
    int base = whence == SEEK_CUR ? (int)f->off : whence == SEEK_END ? (int)f->vn->size : 0;
    if (base + off < 0) return -EINVAL;
    f->off = base + off;
    return (int)f->off;
}
int fd_close(int fd) {
    Task *t = task_current();
    File *f = getf(fd);
    if (!f) return -EBADF;
    t->fds[fd] = NULL; fd_release(f);
    return 0;
}

int fd_dup(int fd) {
    Task *t = task_current();
    File *f = getf(fd);
    if (!f) return -EBADF;
    int n = fd_free_slot(t);
    if (n < 0) return -EMFILE;
    t->fds[n] = file_ref(f);
    return n;
}
int fd_dup2(int oldfd, int newfd) {
    Task *t = task_current();
    File *f = getf(oldfd);
    if (!f) return -EBADF;
    if (newfd < 0 || newfd >= MAXFD) return -EBADF;
    if (oldfd == newfd) return newfd;
    File *old = t->fds[newfd];
    t->fds[newfd] = file_ref(f);
    if (old) fd_release(old);
    return newfd;
}

int fd_pipe(int fds[2], int flags) {
    Task *t = task_current();
    int r = fd_free_slot(t);
    if (r < 0) return -EMFILE;
    t->fds[r] = (File *)1;
    int w = fd_free_slot(t);
    t->fds[r] = NULL;
    if (w < 0) return -EMFILE;
    Pipe *p = pipe_new();
    if (!p) return -ENOMEM;
    File *fr = file_alloc(NULL, O_RDONLY | ((flags & PIPE_RD_NONBLOCK) ? O_NONBLOCK : 0));
    File *fw = file_alloc(NULL, O_WRONLY | ((flags & PIPE_WR_NONBLOCK) ? O_NONBLOCK : 0));
    if (!fr || !fw) { kfree(fr); kfree(fw); kfree(p); return -ENOMEM; }
    fr->pipe = p; fr->pend = 0; fw->pipe = p; fw->pend = 1;
    t->fds[r] = fr; t->fds[w] = fw;
    fds[0] = r; fds[1] = w;
    return 0;
}
