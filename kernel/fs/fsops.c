#include "fs/fsops.h"
#include "fs/vfs.h"
#include "mem/heap.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/errno.h"

#define ORIGINS GOS_TRASH_DIR "/.origins"

static int in_trash(const char *p) {
    size_t l = strlen(GOS_TRASH_DIR);
    return !strncmp(p, GOS_TRASH_DIR, l) && p[l] == '/' && p[l + 1];
}

static void own_user(const char *path, int mode) {
    VNode *n = vfs_lookup(NULL, path);
    if (n) { n->uid = n->gid = GOS_UID_USER; if (mode >= 0) n->mode = (u16)mode; }
}

static void ensure_trash(void) {
    if (vfs_lookup(NULL, GOS_TRASH_DIR)) return;
    vfs_mkdir(NULL, GOS_TRASH_DIR);
    own_user(GOS_TRASH_DIR, 0755);
}

int fs_unique_name(const char *dir, const char *base, const char *suffix, char *out, int max) {
    char b[VFS_NAME], t[VFS_NAME + 8], path[200];
    int sl = (int)strlen(suffix), room = GOS_NAME_MAX - 1 - sl - 4;
    if (room < 1) return -EINVAL;
    strlcpy(b, base, sizeof b);
    if ((int)strlen(b) > room) b[room] = 0;
    for (int i = 1; i < 1000; i++) {
        if (i == 1) ksnprintf(t, sizeof t, "%s%s", b, suffix);
        else ksnprintf(t, sizeof t, "%s%s %d", b, suffix, i);
        ksnprintf(path, sizeof path, "%s/%s", !strcmp(dir, "/") ? "" : dir, t);
        if (!vfs_lookup(NULL, path)) { strlcpy(out, t, (u32)max); return 0; }
    }
    return -EEXIST;
}

static void origins_write(const char *buf, u32 n) {
    vfs_write_file(NULL, ORIGINS, buf, n, 0);
    own_user(ORIGINS, 0644);
}

static void origins_add(const char *tname, const char *orig) {
    char line[260]; int n = ksnprintf(line, sizeof line, "%s\t%s\n", tname, orig);
    vfs_write_file(NULL, ORIGINS, line, (u32)n, 1);
    own_user(ORIGINS, 0644);
}

static int origins_take(const char *tname, char *orig, int max) {
    char *d; u32 l;
    if (vfs_read_file(NULL, ORIGINS, &d, &l)) return -ENOENT;
    char *out = kmalloc(l + 1); int found = -ENOENT; u32 on = 0; size_t tl = strlen(tname);
    if (!out) { kfree(d); return -ENOMEM; }
    for (char *p = d; *p; ) {
        char *e = strchr(p, '\n'); size_t len = e ? (size_t)(e - p) : strlen(p);
        if (found && len > tl && !strncmp(p, tname, tl) && p[tl] == '\t') {
            size_t ol = len - tl - 1; if (ol >= (size_t)max) ol = (size_t)max - 1;
            memcpy(orig, p + tl + 1, ol); orig[ol] = 0; found = 0;
        } else { memcpy(out + on, p, len); on += (u32)len; out[on++] = '\n'; }
        if (!e) break;
        p = e + 1;
    }
    if (!found) origins_write(out, on);
    kfree(out); kfree(d);
    return found;
}

int trash_move(const char *path) {
    VNode *n = vfs_lookup(NULL, path);
    if (!n) return vfs_lookup_errno();
    if (!n->parent || n->virt) return -EPERM;
    if (!strcmp(path, GOS_TRASH_DIR) || in_trash(path)) return -EINVAL;
    ensure_trash();
    char tn[VFS_NAME + 8], to[200];
    int r = fs_unique_name(GOS_TRASH_DIR, n->name, "", tn, sizeof tn);
    if (r) return r;
    ksnprintf(to, sizeof to, "%s/%s", GOS_TRASH_DIR, tn);
    char orig[200]; strlcpy(orig, path, sizeof orig);
    r = vfs_rename(NULL, path, to);
    if (!r) origins_add(tn, orig);
    return r;
}

int trash_putaway(const char *path) {
    VNode *n = vfs_lookup(NULL, path);
    if (!n) return vfs_lookup_errno();
    if (!in_trash(path)) return -EINVAL;
    char orig[200], dest[200], dir[200];
    if (origins_take(n->name, orig, sizeof orig)) ksnprintf(orig, sizeof orig, "/home/user/Desktop/%s", n->name);
    strlcpy(dir, orig, sizeof dir);
    char *sl = strrchr(dir, '/');
    if (sl) { if (sl == dir) dir[1] = 0; else *sl = 0; }
    VNode *dn = vfs_lookup(NULL, dir);
    if (!dn || dn->type != VN_DIR) ksnprintf(orig, sizeof orig, "/home/user/Desktop/%s", n->name);
    strlcpy(dest, orig, sizeof dest);
    if (vfs_lookup(NULL, dest)) {
        origins_add(n->name, dest);
        return -EEXIST;
    }
    return vfs_rename(NULL, path, dest);
}

static int rm_rf(VNode *n) {
    char p[256]; vfs_getpath(n, p, sizeof p);
    if (n->type == VN_DIR) {
        while (n->child) { int r = rm_rf(n->child); if (r) return r; }
        return vfs_rmdir(NULL, p);
    }
    return vfs_unlink(NULL, p);
}

int trash_empty(void) {
    VNode *t = vfs_lookup(NULL, GOS_TRASH_DIR);
    if (!t) return 0;
    int r = 0;
    for (VNode *c = t->child; c && !r; ) {
        VNode *next = c->next;
        if (strcmp(c->name, ".origins")) r = rm_rf(c);
        c = next;
    }
    if (!r && vfs_lookup(NULL, ORIGINS)) origins_write("", 0);
    return r;
}

static u32 tree_bytes(VNode *n) {
    u32 s = n->type == VN_FILE ? n->size : 0;
    for (VNode *c = n->child; c; c = c->next) s += tree_bytes(c);
    return s;
}

void trash_info(u32 *items, u32 *bytes) {
    *items = *bytes = 0;
    VNode *t = vfs_lookup(NULL, GOS_TRASH_DIR);
    if (!t) return;
    for (VNode *c = t->child; c; c = c->next) {
        if (!strcmp(c->name, ".origins")) continue;
        (*items)++; *bytes += tree_bytes(c);
    }
}

static int copy_node(VNode *src, const char *dst, int depth) {
    if (depth > 8) return -ELOOP;
    if (src->virt) return -EPERM;
    if (src->type == VN_DIR) {
        int r = vfs_mkdir(NULL, dst);
        if (r) return r;
        for (VNode *c = src->child; c; c = c->next) {
            char sub[256]; ksnprintf(sub, sizeof sub, "%s/%s", dst, c->name);
            r = copy_node(c, sub, depth + 1);
            if (r) return r;
        }
        return 0;
    }
    int r = vfs_write_file(NULL, dst, src->data, src->size, 0);
    if (!r) { VNode *d = vfs_lookup(NULL, dst); if (d) d->mode = src->mode; }
    return r;
}

int fs_duplicate(const char *path) {
    VNode *n = vfs_lookup(NULL, path);
    if (!n) return vfs_lookup_errno();
    if (!n->parent || n->virt) return -EPERM;
    char dir[200]; vfs_getpath(n->parent, dir, sizeof dir);
    char nm[VFS_NAME + 8], to[256];
    int r = fs_unique_name(dir, n->name, " copy", nm, sizeof nm);
    if (r) return r;
    ksnprintf(to, sizeof to, "%s/%s", !strcmp(dir, "/") ? "" : dir, nm);
    return copy_node(n, to, 0);
}
