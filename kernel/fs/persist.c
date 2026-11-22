#include "fs/persist.h"
#include "fs/vfs.h"
#include "drivers/ata.h"
#include "mem/heap.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/klog.h"
#include "lib/errno.h"
#include "proc/task.h"

static const char MAGIC2[8] = "GOSFS002";
static const char MAGIC1[8] = "GOSFS001";
static u32 saved_gen, last_bytes;
static int cur_slot = -1;
static u32 cur_seq;
static int nslots = 1;
static volatile int sync_busy;
static Task *sync_owner;
static u8 *sync_buf, *sync_img;

typedef struct { char magic[8]; u32 len, sum, seq, hsum; } Hdr2;
typedef struct { char magic[8]; u32 len, sum; } Hdr1;
typedef struct { u16 plen; u8 type; u8 pad; u16 mode; u16 uid; u16 gid; u16 pad2; u32 mtime, size; } __attribute__((packed)) Ent2;
typedef struct { u16 plen; u8 type; u8 pad; u16 mode; u16 pad2; u32 mtime, size; } __attribute__((packed)) Ent1;

static u32 checksum(const u8 *p, u32 n) { u32 s = 0x811C9DC5u; while (n--) s = (s ^ *p++) * 16777619u; return s; }
static u32 slot_lba(int s) { return FS_LBA + (u32)s * FS_SECTORS; }
#define MAX_PAYLOAD ((FS_SECTORS - 1) * 512u)

typedef struct { u8 *p; u32 len, cap; int err; } Out;
static void out_put(Out *o, const void *d, u32 n) {
    if (o->err) return;
    if (o->len + n > o->cap) {
        u32 nc = o->cap ? o->cap * 2 : 4096;
        while (nc < o->len + n) nc *= 2;
        if (o->len + n > MAX_PAYLOAD) { o->err = -ENOSPC; return; }
        if (nc > MAX_PAYLOAD) nc = MAX_PAYLOAD;
        u8 *np = krealloc(o->p, nc);
        if (!np) { o->err = -ENOMEM; return; }
        o->p = np; o->cap = nc;
    }
    memcpy(o->p + o->len, d, n); o->len += n;
}
static void walk_out(Out *o, VNode *dir) {
    for (VNode *c = dir->child; c && !o->err; c = c->next) {
        if (c->virt) continue;
        char path[256]; vfs_getpath(c, path, sizeof path);
        Ent2 e = { (u16)strlen(path), c->type, 0, c->mode, c->uid, c->gid, 0, c->mtime, c->type == VN_FILE ? c->size : 0 };
        out_put(o, &e, sizeof e); out_put(o, path, e.plen);
        if (c->type == VN_FILE) out_put(o, c->data, c->size);
        else walk_out(o, c);
    }
}

int fs_dirty(void) { return vfs_persist_generation() != saved_gen; }

int fs_sync_pid(void) { return sync_busy && sync_owner ? sync_owner->pid : -1; }

void fs_sync_abort(Task *t) {
    if (!sync_busy || sync_owner != t) return;
    if (sync_img) kfree(sync_img);
    if (sync_buf) kfree(sync_buf);
    sync_img = sync_buf = NULL; sync_owner = NULL;
    sync_busy = 0;
    task_wake_obj((void *)&sync_busy);
    kprintf("fs: sync aborted (pid %d killed mid-write)\n", t->pid);
}
u32 fs_last_sync_bytes(void) { return last_bytes; }

#define CHUNK 16

int fs_sync(void) {
    if (!ata_present()) return -EIO;
    if (!fs_dirty()) return 0;

    Task *me = task_current();
    preempt_disable();
    while (sync_busy) {
        if (me->pid == 0) { preempt_enable(); return -EBUSY; }
        task_wait_on((void *)&sync_busy);
    }
    sync_busy = 1; sync_owner = me; sync_buf = sync_img = NULL;
    preempt_enable();

    preempt_disable();
    Out o = { 0 };
    walk_out(&o, vfs_root());
    u32 gen = vfs_persist_generation();
    sync_buf = o.p;
    preempt_enable();

    int r = o.err;
    u8 *img = NULL; u32 secs = 0, total = 0;
    if (!r) {
        total = 512 + o.len; secs = 1 + (o.len + 511) / 512;
        preempt_disable();
        img = kzalloc(secs * 512);
        if (!img) r = -ENOMEM;
        sync_img = img;
        preempt_enable();
    }
    if (!r) {
        int slot = (nslots == 2 && cur_slot >= 0) ? 1 - cur_slot : 0;
        u32 seq = cur_seq + 1, lba = slot_lba(slot);
        Hdr2 *h = (Hdr2 *)img;
        memcpy(h->magic, MAGIC2, 8); h->len = o.len; h->sum = checksum(o.p, o.len); h->seq = seq;
        h->hsum = checksum((const u8 *)h, offsetof(Hdr2, hsum));
        memcpy(img + 512, o.p, o.len);

        for (u32 done = 1; done < secs && !r; ) {
            u32 n = secs - done < CHUNK ? secs - done : CHUNK;
            r = ata_write(lba + done, n, img + done * 512) ? -EIO : 0;
            done += n;
            if (!r && preempt_count() == 0) task_yield();
        }
        if (!r) r = ata_write(lba, 1, img) ? -EIO : 0;
        if (!r) {
            cur_slot = slot; cur_seq = seq; saved_gen = gen; last_bytes = total;
            kprintf("fs: slot %c written (seq %u, %u bytes)\n", 'A' + slot, seq, total);
        }
    }
    preempt_disable();
    if (img) kfree(img);
    if (o.p) kfree(o.p);
    sync_img = sync_buf = NULL; sync_owner = NULL;
    sync_busy = 0;
    task_wake_obj((void *)&sync_busy);
    preempt_enable();
    if (r) kprintf("fs: sync failed (%d)\n", r);
    else kprintf("fs: disk synced (%u bytes)\n", last_bytes);
    return r;
}

static u8 *slot_read(int slot, u32 *seq, u32 *len, int *legacy) {
    u8 sec[512];
    u32 lba = slot_lba(slot);
    if (ata_read(lba, 1, sec)) return NULL;
    u32 plen, sum, skip;
    if (!memcmp(sec, MAGIC2, 8)) {
        Hdr2 *h = (Hdr2 *)sec;
        if (checksum(sec, offsetof(Hdr2, hsum)) != h->hsum) return NULL;
        plen = h->len; sum = h->sum; *seq = h->seq; *legacy = 0; skip = 512;
    } else if (!memcmp(sec, MAGIC1, 8)) {
        Hdr1 *h = (Hdr1 *)sec;
        plen = h->len; sum = h->sum; *seq = 0; *legacy = 1; skip = sizeof(Hdr1);
    } else return NULL;
    if (plen > MAX_PAYLOAD) return NULL;
    u32 total = skip + plen, secs = (total + 511) / 512;
    u8 *img = kmalloc(secs * 512);
    if (!img) return NULL;
    if (ata_read(lba, secs, img)) { kfree(img); return NULL; }
    if (checksum(img + skip, plen) != sum) { kfree(img); return NULL; }
    *len = plen;
    u8 *pay = kmalloc(plen ? plen : 1);
    if (pay) memcpy(pay, img + skip, plen);
    kfree(img);
    return pay;
}

static void legacy_owner(const char *path, u16 *uid, u16 *gid) {
    if (!strncmp(path, "/home/user", 10) || !strncmp(path, "/tmp/", 5)) { *uid = *gid = GOS_UID_USER; }
    else { *uid = *gid = 0; }
}

int fs_load(void) {
    if (!ata_present()) return 0;
    nslots = ata_sectors() >= FS_LBA + FS_SLOTS * FS_SECTORS ? 2 : 1;
    u8 *pay[2] = { NULL, NULL }; u32 seq[2] = { 0, 0 }, len[2] = { 0, 0 }; int leg[2] = { 0, 0 };
    for (int s = 0; s < nslots; s++) {
        pay[s] = slot_read(s, &seq[s], &len[s], &leg[s]);
        if (!pay[s]) kprintf("fs: slot %c empty or corrupt\n", 'A' + s);
    }
    int best = -1;
    for (int s = 0; s < nslots; s++) if (pay[s] && (best < 0 || seq[s] > seq[best])) best = s;
    if (best < 0) return 0;
    for (int s = 0; s < nslots; s++) if (s != best && pay[s]) { kfree(pay[s]); pay[s] = NULL; }

    u8 *p = pay[best]; u32 left = len[best]; int n = 0, legacy = leg[best];
    while (left >= (legacy ? sizeof(Ent1) : sizeof(Ent2))) {
        u16 plen, mode, uid = 0, gid = 0; u8 type; u32 mtime, size;
        if (legacy) { Ent1 e; memcpy(&e, p, sizeof e); p += sizeof e; left -= sizeof e; plen = e.plen; type = e.type; mode = e.mode; mtime = e.mtime; size = e.size; }
        else        { Ent2 e; memcpy(&e, p, sizeof e); p += sizeof e; left -= sizeof e; plen = e.plen; type = e.type; mode = e.mode; uid = e.uid; gid = e.gid; mtime = e.mtime; size = e.size; }
        if (plen + size > left || plen >= 255) break;
        char path[256]; memcpy(path, p, plen); path[plen] = 0; p += plen; left -= plen;
        if (type == VN_DIR) vfs_mkdir(NULL, path);
        else vfs_write_file(NULL, path, p, size, 0);
        VNode *v = vfs_lookup(NULL, path);
        if (v) {
            if (legacy) legacy_owner(path, &uid, &gid);
            v->mode = mode; v->mtime = mtime; v->uid = uid; v->gid = gid;
        }
        p += size; left -= size; n++;
    }
    kfree(pay[best]);
    cur_slot = best; cur_seq = seq[best];
    if (legacy) {
        VNode *t = vfs_lookup(NULL, "/tmp"); if (t) t->mode = 01777;
        VNode *r = vfs_lookup(NULL, "/home"); if (r) { r->uid = r->gid = 0; }
    }
    saved_gen = vfs_persist_generation();
    last_bytes = 512 + len[best];
    kprintf("fs: slot %c loaded (seq %u, %d entries%s)\n", 'A' + best, seq[best], n, legacy ? ", old format" : "");
    if (legacy) saved_gen--;
    return 1;
}

static void syncd(void *arg) {
    (void)arg;
    for (;;) { task_sleep_ms(4000); if (fs_dirty()) fs_sync(); }
}
void syncd_start(void) { task_spawn("syncd", syncd, NULL); }

static void put(const char *path, const char *text) { vfs_write_file(NULL, path, text, strlen(text), 0); }
static void own(const char *path, int uid, int mode) {
    VNode *n = vfs_lookup(NULL, path);
    if (!n) return;
    n->uid = n->gid = (u16)uid;
    if (mode >= 0) n->mode = (u16)mode;
}
static void own_tree(VNode *n, int uid) {
    n->uid = n->gid = (u16)uid;
    for (VNode *c = n->child; c; c = c->next) own_tree(c, uid);
}

static const char OS_RELEASE[] = "NAME=\"GOS\"\nVERSION=" GOS_VERSION "\nID=gos\nPRETTY_NAME=\"GOS - Green Operating System\"\n";

void fs_refresh_system(void) {
    VNode *n = vfs_lookup(NULL, "/etc/os-release");
    if (!n || n->size != sizeof OS_RELEASE - 1 || memcmp(n->data, OS_RELEASE, n->size)) {
        put("/etc/os-release", OS_RELEASE); own("/etc/os-release", 0, 0644);
    }
    if (!vfs_lookup(NULL, "/root")) { vfs_mkdir(NULL, "/root"); own("/root", 0, 0700); }
    if (!vfs_lookup(NULL, "/home/user/Desktop/Root Terminal.app") && vfs_lookup(NULL, "/home/user/Desktop")) {
        put("/home/user/Desktop/Root Terminal.app", "root:app:terminal\n");
        own("/home/user/Desktop/Root Terminal.app", 0, 0644);
    }
    VNode *t = vfs_lookup(NULL, "/tmp");
    if (t && (t->mode & 01777) != 01777) t->mode = 01777;
    static const char *const udirs[] = { "/home/user/Trash", "/home/user/Documents" };
    for (int i = 0; i < 2; i++) {
        if (vfs_lookup(NULL, "/home/user") && !vfs_lookup(NULL, udirs[i])) {
            vfs_mkdir(NULL, udirs[i]);
            VNode *d = vfs_lookup(NULL, udirs[i]); if (d) d->uid = d->gid = GOS_UID_USER;
        }
    }
}

void fs_make_defaults(void) {
    const char *dirs[] = { "/etc", "/home", "/home/user", "/home/user/Desktop", "/home/user/Documents",
                           "/tmp", "/var", "/var/log", "/usr", "/usr/share", "/root" };
    for (int i = 0; i < ARRAY_LEN(dirs); i++) vfs_mkdir(NULL, dirs[i]);
    put("/etc/hostname", "gos\n");
    put("/etc/passwd", "root:x:0:0:root:/root:/bin/sh\nuser:x:1000:1000:user:/home/user:/bin/sh\n");
    put("/etc/motd", "Welcome to GOS - Green Operating System!\nType 'help' for a list of commands.\n");
    put("/etc/profile", "# read when a terminal opens\nexport EDITOR=edit\n");
    put("/etc/autostart", "# programs to start at every boot, one per line (names from /bin). Example: ticker\n");
    put("/home/user/README.txt",
        "GOS " GOS_VERSION " - a hobby Unix-like OS\n"
        "=============================\n\n"
        "Terminal   : Unix-style commands (ls cat grep ... | > >> < $VAR &). Type 'help'.\n"
        "             ELF programs in /bin can be piped: cat x | upper, and $? holds their exit code.\n"
        "Files      : / is the root. /dev, /proc and /bin are virtual. File permissions (rwx, user/root) apply.\n"
        "Disk       : files are saved to disk automatically, alternating between two slots A/B ('sync' forces a write).\n"
        "Windows    : drag the title bar, resize from the bottom-right corner, click the box\n"
        "             at the top right to zoom. Alt+Tab switches windows.\n"
        "Shortcuts  : Alt+T terminal, Alt+F file manager, Alt+E editor, Alt+M monitor.\n"
        "Desktop    : the contents of /home/user/Desktop show up as icons. Make a .app file containing\n"
        "             'app:terminal', 'app:files /tmp', 'app:edit /etc/motd' or 'app:sysmon'.\n"
        "Root       : the 'Root Terminal' icon opens a terminal as root (uid 0). Be careful.\n"
        "\n"
        "System 6 style (v0.6)\n"
        "Apple menu : About, desk accessories (Alarm Clock, Calculator, Control Panel, Key Caps, Note Pad, Puzzle, Scrapbook).\n"
        "Trash      : drag an icon to the Trash (or press Del); Special > Empty Trash. Put Away in the File Manager restores it.\n"
        "Finder     : View by Icon/Name/Date/Size/Kind, Get Info, Duplicate, Find; drag items between windows and the desktop.\n"
        "Apps       : the menu at the right of the menu bar lists the windows, plus Hide / Hide Others / Show All (MultiFinder style).\n");
    put("/home/user/hello.sh",
        "#!/bin/sh\n# example GOS shell script\necho \"Hello from the script, $USER!\"\nuname -a\nuptime\nls /\n");
    put("/home/user/notes.txt", "Notes:\n- try: cat /proc/meminfo\n- try: spawn ticker, then open /tmp/ticker.txt\n- try: ps | grep ticker\n- try: echo hello | upper\n");
    put("/home/user/Desktop/Terminal.app", "app:terminal\n");
    put("/home/user/Desktop/Files.app", "app:files /home/user\n");
    put("/home/user/Desktop/Editor.app", "app:edit /home/user/notes.txt\n");
    put("/home/user/Desktop/Monitor.app", "app:sysmon\n");
    put("/home/user/Desktop/Paint.app", "app:paint\n");
    put("/home/user/Desktop/README.txt", "This folder's contents are shown as icons on the desktop.\n");
    VNode *h = vfs_lookup(NULL, "/home/user"); if (h) own_tree(h, GOS_UID_USER);
    own("/tmp", 0, 01777); own("/root", 0, 0700);
    fs_refresh_system();
}
