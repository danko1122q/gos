#include "sys/syscall.h"
#include "arch/idt.h"
#include "arch/power.h"
#include "arch/timer.h"
#include "drivers/rtc.h"
#include "drivers/ata.h"
#include "fs/vfs.h"
#include "fs/persist.h"
#include "gui/window.h"
#include "gui/desktop.h"
#include "gui/dialog.h"
#include "gfx/font.h"
#include "mem/uaccess.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "proc/task.h"
#include "proc/exec.h"
#include "sys/clip.h"
#include "sys/settings.h"
#include "drivers/speaker.h"
#include "fs/fsops.h"
#include "lib/errno.h"
#include "lib/klog.h"
#include "lib/string.h"

#define PATH_MAX 128

static int ustr(const char *u, char *k, int max) {
    int n = strncpy_from_user(k, u, (u32)max);
    return n < 0 ? n : 0;
}

static int sys_read(int fd, void *ubuf, u32 n) {
    if (!uaccess_ok(ubuf, n, 1)) return -EFAULT;
    char kb[512]; u32 total = 0;
    while (n) {
        u32 c = n < sizeof kb ? n : sizeof kb;
        int r = fd_read(fd, kb, c);
        if (r < 0) return total ? (int)total : r;
        if (r == 0) break;
        if (copy_to_user((char *)ubuf + total, kb, (u32)r)) return -EFAULT;
        total += (u32)r;
        if ((u32)r < c) break;
        n -= (u32)r;
    }
    return (int)total;
}

static int sys_write(int fd, const void *ubuf, u32 n) {
    if (!uaccess_ok(ubuf, n, 0)) return -EFAULT;
    char kb[512]; u32 total = 0;
    while (n) {
        u32 c = n < sizeof kb ? n : sizeof kb;
        if (copy_from_user(kb, (const char *)ubuf + total, c)) return -EFAULT;
        int r = fd_write(fd, kb, c);
        if (r < 0) return total ? (int)total : r;
        total += (u32)r;
        if ((u32)r < c) break;
        n -= (u32)r;
    }
    return (int)total;
}

static int sys_stat(Task *t, const char *upath, GStat *ust) {
    char p[PATH_MAX]; int r = ustr(upath, p, sizeof p); if (r) return r;
    VNode *n = vfs_lookup(t->cwd, p);
    if (!n) return vfs_lookup_errno();
    GStat st; memset(&st, 0, sizeof st);
    st.type = n->type; st.virt = n->virt; st.mode = n->mode; st.size = n->size; st.mtime = n->mtime; st.ino = n->ino;
    st.uid = n->uid; st.gid = n->gid;
    return copy_to_user(ust, &st, sizeof st);
}

static int sys_fstat(int fd, GStat *ust) {
    File *f = fd_file(fd);
    if (!f) return -EBADF;
    GStat st; memset(&st, 0, sizeof st);
    if (f->pipe) {
        st.type = VN_PIPE;
        st.mode = 0600;
    } else if (f->vn) {
        st.type = f->vn->type; st.virt = f->vn->virt; st.mode = f->vn->mode;
        st.size = f->vn->size; st.mtime = f->vn->mtime; st.ino = f->vn->ino;
        st.uid = f->vn->uid; st.gid = f->vn->gid;
    } else return -EBADF;
    return copy_to_user(ust, &st, sizeof st);
}

static int sys_readdir(Task *t, const char *upath, int idx, GDirEnt *uent) {
    char p[PATH_MAX]; int r = ustr(upath, p, sizeof p); if (r) return r;
    VNode *d = vfs_lookup(t->cwd, p);
    if (!d) return vfs_lookup_errno();
    if (d->type != VN_DIR) return -ENOTDIR;
    if ((r = vfs_access(d, P_R))) return r;
    VNode *c = d->child;
    for (int i = 0; c && i < idx; i++) c = c->next;
    if (!c) return 0;
    GDirEnt e; memset(&e, 0, sizeof e);
    strlcpy(e.name, c->name, sizeof e.name); e.type = c->type; e.size = c->size;
    r = copy_to_user(uent, &e, sizeof e);
    return r ? r : 1;
}

static int sys_getcwd(Task *t, char *ubuf, int size) {
    char p[256];
    int l = vfs_getpath(t->cwd ? t->cwd : vfs_root(), p, sizeof p);
    if (l + 1 > size) return -ERANGE;
    int r = copy_to_user(ubuf, p, (u32)l + 1);
    return r ? r : l;
}

static int sys_spawn(Task *t, const char *upath, const char *const *uargv, const int *ufdmap, int flags) {
    char path[PATH_MAX]; int r = ustr(upath, path, sizeof path); if (r) return r;
    char args[16][96]; char *av[16]; int ac = 0;
    if (uargv) {
        for (; ac < 15; ac++) {
            u32 ptr;
            if (copy_from_user(&ptr, uargv + ac, 4)) return -EFAULT;
            if (!ptr) break;
            r = strncpy_from_user(args[ac], (const char *)ptr, sizeof args[0]);
            if (r < 0) return r;
            av[ac] = args[ac];
        }
    }
    if (ac == 0) { const char *b = strrchr(path, '/'); strlcpy(args[0], b ? b + 1 : path, sizeof args[0]); av[0] = args[0]; ac = 1; }
    int fdmap[3] = { GOS_FD_INHERIT, GOS_FD_INHERIT, GOS_FD_INHERIT };
    if (ufdmap && copy_from_user(fdmap, ufdmap, sizeof fdmap)) return -EFAULT;
    int parent = (flags & SPAWN_DETACH) ? 0 : t->pid;
    return proc_spawn_ex(path, ac, av, parent, t->uid, t->gid, fdmap);
}


static int sys_wait(int pid, int *ustatus, int flags) {
    if (ustatus && !uaccess_ok(ustatus, sizeof *ustatus, 1)) return -EFAULT;
    int code = 0;
    int r = task_wait(pid, &code, flags);
    if (r > 0 && ustatus && copy_to_user(ustatus, &code, sizeof code)) return -EFAULT;
    return r;
}

static int sys_pipe(int *ufds, int flags) {
    if (!uaccess_ok(ufds, 2 * sizeof(int), 1)) return -EFAULT;
    int fds[2];
    int r = fd_pipe(fds, flags);
    if (r) return r;
    if (copy_to_user(ufds, fds, sizeof fds)) { fd_close(fds[0]); fd_close(fds[1]); return -EFAULT; }
    return 0;
}

static int sys_chown(Task *t, const char *upath, int uid, int gid) {
    char p[PATH_MAX]; int r = ustr(upath, p, sizeof p); if (r) return r;
    VNode *n = vfs_lookup(t->cwd, p);
    if (!n) return vfs_lookup_errno();
    return vfs_chown(n, (u16)uid, (u16)gid);
}

static int sys_chmod(Task *t, const char *upath, int mode) {
    char p[PATH_MAX]; int r = ustr(upath, p, sizeof p); if (r) return r;
    VNode *n = vfs_lookup(t->cwd, p);
    if (!n) return vfs_lookup_errno();
    return vfs_chmod(n, (u16)mode);
}

static int sys_kill(Task *t, int pid, int sig) {
    (void)t; (void)sig;
    return task_kill(pid);
}

static int sys_procinfo(int slot, GProcInfo *u) {
    Task *t = task_get(slot);
    if (!t) return -ESRCH;
    GProcInfo pi; memset(&pi, 0, sizeof pi);
    pi.pid = t->pid; pi.state = t->state; pi.user = t->pd != 0; pi.cpu = t->cpu;
    pi.mem_kb = t->pd ? vmm_count_user_pages(t->pd) * 4 : 0;
    pi.ppid = t->parent; pi.uid = t->uid; pi.exit_code = t->exit_code;
    strlcpy(pi.name, t->name, sizeof pi.name);
    return copy_to_user(u, &pi, sizeof pi);
}

static int sys_sysinfo(GSysInfo *u) {
    GSysInfo s; memset(&s, 0, sizeof s);
    s.ram_kb = mem_total_kb(); s.heap_total = heap_total(); s.heap_used = heap_used();
    s.pmm_total = pmm_total_frames(); s.pmm_free = pmm_free_frames();
    s.ticks = timer_ticks(); s.hz = HZ; s.cpu_load = cpu_load_pct();
    s.ata = (u32)ata_present(); s.fs_dirty = (u32)fs_dirty(); s.fs_sync_bytes = fs_last_sync_bytes();
    s.vfs_gen = vfs_generation(); vfs_stats(&s.vfs_files, &s.vfs_dirs, &s.vfs_bytes);
    s.nwin = (u32)wm_count();
    s.fs_sync_pid = fs_sync_pid();
    return copy_to_user(u, &s, sizeof s);
}

static int sys_klog(char *ubuf, int max) {
    if (max <= 0 || !uaccess_ok(ubuf, (u32)max, 1)) return -EFAULT;
    if (max > KLOG_SIZE) max = KLOG_SIZE;
    char *kb = kmalloc((u32)max);
    if (!kb) return -ENOMEM;
    int n = klog_read(kb, max);
    int r = copy_to_user(ubuf, kb, (u32)n) ? -EFAULT : n;
    kfree(kb);
    return r;
}

static int sys_clip_set(const void *ubuf, u32 n) {
    if (n > GOS_CLIP_MAX) return -EFBIG;
    if (!uaccess_ok(ubuf, n, 0)) return -EFAULT;
    char *kb = n ? kmalloc(n) : NULL;
    if (n && !kb) return -ENOMEM;
    int r = n && copy_from_user(kb, ubuf, n) ? -EFAULT : 0;
    if (!r) r = clip_set(kb, n);
    if (kb) kfree(kb);
    return r;
}

static int sys_clip_get(void *ubuf, u32 max) {
    u32 total = clip_len();
    if (!ubuf || !max) return (int)total;
    u32 n = total < max ? total : max;
    if (!uaccess_ok(ubuf, n, 1)) return -EFAULT;
    char *kb = n ? kmalloc(n) : NULL;
    if (n && !kb) return -ENOMEM;
    if (n) clip_get(kb, n);
    int r = n && copy_to_user(ubuf, kb, n) ? -EFAULT : (int)total;
    if (kb) kfree(kb);
    return r;
}

static int sys_win_create(Task *t, const char *utitle, int x, int y, int w, int h) {
    char title[40]; int r = ustr(utitle, title, sizeof title);
    if (r == -ENAMETOOLONG) { strncpy_from_user(title, utitle, 39); title[39] = 0; r = 0; }
    if (r) return r;
    if (w < 100) w = 100;
    if (h < 60) h = 60;
    Window *win = uwin_create(t, title, x, y, w, h);
    return win ? win->id : -ENOMEM;
}

static int sys_win_event(Task *t, int id, GEvent *uev, int timeout) {
    preempt_disable();
    Window *w = uwin_find(t, id);
    preempt_enable();
    if (!w) return -EBADF;
    if (!uaccess_ok(uev, sizeof *uev, 1)) return -EFAULT;
    GEvent e;
    int r = uwin_wait(w, &e, timeout);
    if (r > 0 && copy_to_user(uev, &e, sizeof e)) return -EFAULT;
    return r;
}

static int sys_win_ctl(Task *t, int id, int cmd, int a, int b, int c) {
    Window *w = uwin_find(t, id);
    if (!w) return -EBADF;
    char s1[256], s2[128]; int r;
    switch (cmd) {
    case WC_TITLE: case WC_INFO: case WC_EDITMENU:
        r = strncpy_from_user(s1, (const char *)a, sizeof s1);
        if (r == -ENAMETOOLONG) r = 0;
        if (r < 0) return r;
        return uwin_ctl(w, cmd, s1, 0, 0, 0);
    case WC_MENU:
        s1[0] = s2[0] = 0;
        if (a) { r = strncpy_from_user(s1, (const char *)a, sizeof s1); if (r < 0 && r != -ENAMETOOLONG) return r; }
        if (b) { r = strncpy_from_user(s2, (const char *)b, sizeof s2); if (r < 0 && r != -ENAMETOOLONG) return r; }
        return uwin_set_menu(w, s1, s2);
    default:
        return uwin_ctl(w, cmd, NULL, a, b, c);
    }
}

static int sys_msgbox(const char *ut, const char *u1, const char *u2) {
    char t[40], l1[64], l2[64];
    t[0] = l1[0] = l2[0] = 0;
    int r;
    if (ut) { r = strncpy_from_user(t, ut, sizeof t);  if (r < 0 && r != -ENAMETOOLONG) return r; }
    if (u1) { r = strncpy_from_user(l1, u1, sizeof l1); if (r < 0 && r != -ENAMETOOLONG) return r; }
    if (u2) { r = strncpy_from_user(l2, u2, sizeof l2); if (r < 0 && r != -ENAMETOOLONG) return r; }
    preempt_disable();
    dialog_msg(t, l1, l2);
    desktop_invalidate();
    preempt_enable();
    return 0;
}


static int sys_trash(Task *t, int op, const char *upath, u32 *uout) {
    char p[PATH_MAX], a[PATH_MAX]; int r;
    switch (op) {
    case TRASH_EMPTY: return trash_empty();
    case TRASH_INFO: { u32 o[2]; trash_info(&o[0], &o[1]); return copy_to_user(uout, o, sizeof o); }
    case TRASH_MOVE: case TRASH_PUTAWAY:
        if ((r = ustr(upath, p, sizeof p))) return r;
        if (vfs_join(t->cwd, p, a, sizeof a)) return -EINVAL;
        return op == TRASH_MOVE ? trash_move(a) : trash_putaway(a);
    }
    return -EINVAL;
}

static int sys_duplicate(Task *t, const char *upath) {
    char p[PATH_MAX], a[PATH_MAX]; int r = ustr(upath, p, sizeof p);
    if (r) return r;
    if (vfs_join(t->cwd, p, a, sizeof a)) return -EINVAL;
    return fs_duplicate(a);
}

static int sys_drag_begin(Task *t, const char *upath) {
    char p[PATH_MAX], a[PATH_MAX]; int r = ustr(upath, p, sizeof p);
    if (r) return r;
    if (vfs_join(t->cwd, p, a, sizeof a)) return -EINVAL;
    return desktop_drag_begin(t, a);
}

static int sys_drag_get(char *ubuf, int max) {
    char tmp[128]; int n = desktop_drag_path(tmp, sizeof tmp);
    if (!ubuf || max <= 0) return n;
    if (n + 1 > max) return -ERANGE;
    int r = copy_to_user(ubuf, tmp, (u32)n + 1);
    return r ? r : n;
}

static int sys_confirm(const char *ut, const char *u1, const char *u2, const char *uok) {
    char t[40], l1[64], l2[64], ok[16]; int r;
    t[0] = l1[0] = l2[0] = ok[0] = 0;
    if (ut)  { r = strncpy_from_user(t, ut, sizeof t);   if (r < 0 && r != -ENAMETOOLONG) return r; }
    if (u1)  { r = strncpy_from_user(l1, u1, sizeof l1); if (r < 0 && r != -ENAMETOOLONG) return r; }
    if (u2)  { r = strncpy_from_user(l2, u2, sizeof l2); if (r < 0 && r != -ENAMETOOLONG) return r; }
    if (uok) { r = strncpy_from_user(ok, uok, sizeof ok); if (r < 0 && r != -ENAMETOOLONG) return r; }
    u32 tok;
    for (;;) {
        preempt_disable();
        if (!dialog_visible()) { tok = dialog_ask(t, l1, l2, ok[0] ? ok : NULL, 0); desktop_invalidate(); preempt_enable(); break; }
        preempt_enable();
        task_sleep_ms(30);
    }
    int res;
    while ((res = dialog_ask_result(tok)) < 0) task_sleep_ms(30);
    return res;
}

static int blocking_call(u32 n) {
    switch (n) {
    case SYS_READ: case SYS_WRITE: case SYS_WAIT: case SYS_SLEEPMS: case SYS_YIELD:
    case SYS_SYNC: case SYS_REBOOT: case SYS_POWEROFF: case SYS_WIN_EVENT: case SYS_WIN_MSGBOX: case SYS_EXIT: case SYS_CONFIRM:
        return 1;
    }
    return 0;
}

static int dispatch(Task *t, Regs *r) {
    int a = (int)REGS_ARG0(r), b = (int)REGS_ARG1(r), c = (int)REGS_ARG2(r), d = (int)REGS_ARG3(r), ret = -ENOSYS;
    char p[PATH_MAX], q[PATH_MAX]; int e;
    switch (REGS_SYSNO(r)) {
    case SYS_EXIT:   task_exit(a & 0xFF);
    case SYS_READ:   ret = sys_read(a, (void *)b, (u32)c); break;
    case SYS_WRITE:  ret = sys_write(a, (const void *)b, (u32)c); break;
    case SYS_OPEN:   ret = (e = ustr((const char *)a, p, sizeof p)) ? e : fd_open(p, b); break;
    case SYS_CLOSE:  ret = fd_close(a); break;
    case SYS_DUP:    ret = fd_dup(a); break;
    case SYS_DUP2:   ret = fd_dup2(a, b); break;
    case SYS_PIPE:   ret = sys_pipe((int *)a, b); break;
    case SYS_WAIT:   ret = sys_wait(a, (int *)b, c); break;
    case SYS_GETUID: ret = t->uid; break;
    case SYS_GETGID: ret = t->gid; break;
    case SYS_CHOWN:  ret = sys_chown(t, (const char *)a, b, c); break;
    case SYS_UNLINK: ret = (e = ustr((const char *)a, p, sizeof p)) ? e : vfs_unlink(t->cwd, p); break;
    case SYS_MKDIR:  ret = (e = ustr((const char *)a, p, sizeof p)) ? e : vfs_mkdir(t->cwd, p); break;
    case SYS_RMDIR:  ret = (e = ustr((const char *)a, p, sizeof p)) ? e : vfs_rmdir(t->cwd, p); break;
    case SYS_LSEEK:  ret = fd_lseek(a, b, c); break;
    case SYS_GETPID: ret = t->pid; break;
    case SYS_TIME:   ret = (int)time_now(); break;
    case SYS_YIELD:  task_yield(); ret = 0; break;
    case SYS_SLEEPMS: task_sleep_ms((u32)(a < 0 ? 0 : a)); ret = 0; break;
    case SYS_CHDIR:
        if ((e = ustr((const char *)a, p, sizeof p))) { ret = e; break; }
        { VNode *n = vfs_lookup(t->cwd, p);
          if (!n) ret = vfs_lookup_errno();
          else if (n->type != VN_DIR) ret = -ENOTDIR;
          else if ((ret = vfs_access(n, P_X))) break;
          else { t->cwd = n; ret = 0; } }
        break;
    case SYS_GETCWD: ret = sys_getcwd(t, (char *)a, b); break;
    case SYS_CHMOD:  ret = sys_chmod(t, (const char *)a, b); break;
    case SYS_SYNC:   ret = fs_sync(); break;
    case SYS_KILL:   ret = sys_kill(t, a, b); break;
    case SYS_RENAME:
        if ((e = ustr((const char *)a, p, sizeof p)) || (e = ustr((const char *)b, q, sizeof q))) { ret = e; break; }
        ret = vfs_rename(t->cwd, p, q);
        break;
    case SYS_BRK:    ret = proc_brk(t, (u32)a); break;
    case SYS_STAT:   ret = sys_stat(t, (const char *)a, (GStat *)b); break;
    case SYS_FSTAT:  ret = sys_fstat(a, (GStat *)b); break;
    case SYS_READDIR: ret = sys_readdir(t, (const char *)a, b, (GDirEnt *)c); break;
    case SYS_SPAWN:  ret = sys_spawn(t, (const char *)a, (const char *const *)b, (const int *)c, d); break;
    case SYS_PROCINFO: ret = sys_procinfo(a, (GProcInfo *)b); break;
    case SYS_SYSINFO: ret = sys_sysinfo((GSysInfo *)a); break;
    case SYS_KLOG:   ret = sys_klog((char *)a, b); break;
    case SYS_REBOOT: fs_sync(); sys_reboot(); break;
    case SYS_POWEROFF:
#ifdef GOS_STACKMARK
        { u32 hw = task_stack_hw();
          klog("stack-hw: deepest kernel stack use %u of %u bytes\n", hw, (u32)STACK_SZ);
          if (hw > STACK_SZ / 4 * 3) klog("selftest: FAIL kernel stack use > 75%% (%u of %u bytes)\n", hw, (u32)STACK_SZ); }
#endif
        fs_sync(); sys_poweroff(); break;
    case SYS_LAUNCH:
        if ((e = ustr((const char *)a, p, sizeof p))) { ret = e; break; }
        app_open_path(p); ret = 0;
        break;
    case SYS_GETFONT: ret = copy_to_user((void *)a, font_get_base(), 4096); break;
    case SYS_GETUIFONT: {
        const UiFont *uf = font_get_ui(a);
        ret = uf ? copy_to_user((void *)b, uf, sizeof(UiFont)) : -EINVAL;
        break;
    }
    case SYS_TICKS:  ret = (int)timer_ticks(); break;
    case SYS_WIN_CREATE: ret = sys_win_create(t, (const char *)a, b, c, d, (int)REGS_ARG4(r)); break;
    case SYS_WIN_INFO: {
        Window *w = uwin_find(t, a);
        if (!w) { ret = -EBADF; break; }
        GWinInfo gi; uwin_info(w, &gi);
        ret = copy_to_user((void *)b, &gi, sizeof gi);
        break; }
    case SYS_WIN_EVENT:   ret = sys_win_event(t, a, (GEvent *)b, c); break;
    case SYS_WIN_PRESENT: ret = uwin_find(t, a) ? 0 : -EBADF; if (!ret) desktop_invalidate(); break;
    case SYS_WIN_CTL:     ret = sys_win_ctl(t, a, b, c, d, (int)REGS_ARG4(r)); break;
    case SYS_WIN_CLOSE: { Window *w = uwin_find(t, a); if (!w) ret = -EBADF; else { uwin_close(w); ret = 0; } break; }
    case SYS_CLIP_SET: ret = sys_clip_set((const void *)a, (u32)b); break;
    case SYS_CLIP_GET: ret = sys_clip_get((void *)a, (u32)b); break;
    case SYS_WIN_MSGBOX: ret = sys_msgbox((const char *)a, (const char *)b, (const char *)c); break;
    case SYS_SETTING_GET: ret = (a >= 0 && a < SET_COUNT) ? settings_get(a) : -EINVAL; break;
    case SYS_SETTING_SET: ret = settings_set(a, b); break;
    case SYS_SETTIME: ret = rtc_set((u32)a); break;
    case SYS_BEEP: if (a == 0) desktop_alert(); else speaker_beep((u32)a, (u32)(b < 0 ? 0 : b)); ret = 0; break;
    case SYS_TRASH: ret = sys_trash(t, a, (const char *)b, (u32 *)c); break;
    case SYS_DUPLICATE: ret = sys_duplicate(t, (const char *)a); break;
    case SYS_DRAG_BEGIN: ret = sys_drag_begin(t, (const char *)a); break;
    case SYS_DRAG_GET: ret = sys_drag_get((char *)a, b); break;
    case SYS_CONFIRM: ret = sys_confirm((const char *)a, (const char *)b, (const char *)c, (const char *)d); break;
    }
    return ret;
}

static void handler(Regs *r) {
    Task *t = task_current();
    int own = !blocking_call(REGS_SYSNO(r));
    if (own) preempt_disable();
    int ret = dispatch(t, r);
    if (own) preempt_enable();
    REGS_RET(r) = (u32)ret;
}

void syscall_init(void) { isr_register(0x80, handler); }
