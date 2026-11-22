#include "proc/exec.h"
#include "proc/task.h"
#include "mem/vmm.h"
#include "mem/pmm.h"
#include "mem/heap.h"
#include "fs/vfs.h"
#include "arch/idt.h"
#include "arch/io.h"
#include "arch/arch.h"
#include "gui/dialog.h"
#include "gui/desktop.h"
#include "gui/window.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/klog.h"
#include "lib/errno.h"

typedef struct { u8 ident[16]; u16 type, machine; u32 version, entry, phoff, shoff, flags;
                 u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx; } Elf32Ehdr;
typedef struct { u32 type, offset, vaddr, paddr, filesz, memsz, flags, align; } Elf32Phdr;
enum { PT_LOAD = 1, PF_X = 1, PF_W = 2, PF_R = 4 };

#define MAX_ARGC 16
#define MAX_ARGB 1536

static int poke(u32 pd, u32 va, const void *src, u32 n) {
    const u8 *s = src;
    while (n) {
        u32 e = vmm_walk(pd, va);
        if (!(e & PTE_P)) return -EFAULT;
        u32 off = va & (PAGE_SIZE - 1), c = PAGE_SIZE - off;
        if (c > n) c = n;
        memcpy((void *)((e & PAGE_MASK) + off), s, c);
        va += c; s += c; n -= c;
    }
    return 0;
}

static int load_elf(u32 pd, const u8 *img, u32 len, u32 *entry, u32 *brk, u32 *lazy_lo) {
    const Elf32Ehdr *eh = (const Elf32Ehdr *)img;
    if (len < sizeof *eh || memcmp(eh->ident, "\x7f" "ELF", 4) || eh->ident[4] != 1 || eh->ident[5] != 1) return -ENOEXEC;
    if (eh->type != 2 || eh->machine != GOS_ELF_MACHINE || eh->phentsize != sizeof(Elf32Phdr) || !eh->phnum || eh->phnum > 8) return -ENOEXEC;
    if (eh->phoff > len || eh->phnum * sizeof(Elf32Phdr) > len - eh->phoff) return -ENOEXEC;
    u32 top = 0, lazy = 0;
    for (int i = 0; i < eh->phnum; i++) {
        const Elf32Phdr *ph = (const Elf32Phdr *)(img + eh->phoff) + i;
        if (ph->type != PT_LOAD || !ph->memsz || ph->vaddr < USER_BASE || ph->vaddr + ph->memsz > USER_BRK_LIMIT) continue;
        u32 e = (ph->vaddr + ph->memsz + PAGE_SIZE - 1) & PAGE_MASK;
        if (e > top) top = e;
    }
    u32 ztop = top; top = 0;
    for (int i = 0; i < eh->phnum; i++) {
        const Elf32Phdr *ph = (const Elf32Phdr *)(img + eh->phoff) + i;
        if (ph->type != PT_LOAD || !ph->memsz) continue;
        if (ph->vaddr < USER_BASE || ph->memsz > 0x4000000u || ph->vaddr + ph->memsz > USER_BRK_LIMIT) return -ENOEXEC;
        if (ph->filesz > ph->memsz || ph->offset > len || ph->filesz > len - ph->offset) return -ENOEXEC;
        u32 va0 = ph->vaddr & PAGE_MASK, va1 = (ph->vaddr + ph->memsz + PAGE_SIZE - 1) & PAGE_MASK;
        u32 fe = (ph->vaddr + ph->filesz + PAGE_SIZE - 1) & PAGE_MASK;
        int lazy_seg = (ph->flags & PF_W) && va1 == ztop && fe < va1;
        if (lazy_seg) lazy = fe;
        for (u32 va = va0; va < va1; va += PAGE_SIZE) {
            if (lazy_seg && va >= fe) break;
            u32 e = vmm_walk(pd, va);
            if (e & PTE_P) { if ((ph->flags & PF_W) && !(e & PTE_W)) vmm_map(pd, va, e & PAGE_MASK, PTE_U | PTE_W); continue; }
            u32 f = pmm_alloc_zero();
            if (!f) return -ENOMEM;
            if (vmm_map(pd, va, f, PTE_U | ((ph->flags & PF_W) ? PTE_W : 0)) < 0) { pmm_free(f); return -ENOMEM; }
        }
        int r = poke(pd, ph->vaddr, img + ph->offset, ph->filesz);
        if (r) return r;
        if (va1 > top) top = va1;
    }
    if (!top || eh->entry < USER_BASE || eh->entry >= top) return -ENOEXEC;
    *entry = eh->entry; *brk = top; *lazy_lo = lazy;
    return 0;
}

static void default_ids(u16 *uid, u16 *gid) {
    Task *c = task_current();
    if (!c || c->pid == 0) { *uid = GOS_UID_USER; *gid = GOS_UID_USER; }
    else { *uid = c->uid; *gid = c->gid; }
}

int proc_spawn(const char *path, int argc, char **argv) {
    u16 uid, gid; default_ids(&uid, &gid);
    return proc_spawn_ex(path, argc, argv, 0, uid, gid, NULL);
}

int proc_spawn_ex(const char *path, int argc, char **argv, int parent, u16 uid, u16 gid, const int *fdmap) {
    Task *cur = task_current();
    File *std[3] = { NULL, NULL, NULL };
    if (fdmap) {
        for (int i = 0; i < 3; i++) {
            if (fdmap[i] == GOS_FD_INHERIT) continue;
            std[i] = fd_file(fdmap[i]);
            if (!std[i]) return -EBADF;
        }
    }
    VNode *vn = vfs_lookup(cur ? cur->cwd : NULL, path);
    if (!vn) return vfs_lookup_errno();
    if (vn->type != VN_FILE) return -EACCES;
    int ar = vfs_access(vn, P_X);
    if (ar) return ar;
    char *img; u32 len;
    int r = vfs_read_file(cur ? cur->cwd : NULL, path, &img, &len);
    if (r) return r;
    if (argc > MAX_ARGC) argc = MAX_ARGC;
    u32 pd = vmm_create_space();
    if (!pd) { kfree(img); return -ENOMEM; }
    u32 entry, brk, lazy_lo;
    r = load_elf(pd, (u8 *)img, len, &entry, &brk, &lazy_lo);
    kfree(img);
    if (!r) r = vmm_alloc_map(pd, USER_STACK_TOP - USER_STACK_INIT * PAGE_SIZE, USER_STACK_INIT, PTE_U | PTE_W);
    u32 sp = USER_STACK_TOP;
    if (!r) {
        u32 av[MAX_ARGC] = { 0 }, used = 0;
        for (int i = argc - 1; i >= 0 && !r; i--) {
            u32 l = (u32)strlen(argv[i]) + 1;
            if (used + l > MAX_ARGB) { r = -E2BIG; break; }
            used += l; sp -= l;
            r = poke(pd, sp, argv[i], l); av[i] = sp;
        }
        if (!r) {
            sp &= ~15u;
            u32 w[MAX_ARGC + 2], n = 0;
            w[n++] = (u32)argc;
            for (int i = 0; i < argc; i++) w[n++] = av[i];
            w[n++] = 0;
            sp -= n * 4;
            r = poke(pd, sp, w, n * 4);
        }
    }
    if (r) { vmm_destroy_space(pd); return r; }
    const char *b = strrchr(path, '/'); b = b ? b + 1 : path;
    int pid = task_create_user(b, pd, entry, sp, brk, lazy_lo, parent, uid, gid, std);
    if (pid < 0) { vmm_destroy_space(pd); return -EAGAIN; }
    return pid;
}

#define BRK_RESERVE_FRAMES 16u
int proc_brk(Task *t, u32 nb) {
    if (nb == 0) return (int)t->brk;
    if (nb < t->brk_start || nb > USER_BRK_LIMIT) return (int)t->brk;
    u32 old = (t->brk + PAGE_SIZE - 1) & PAGE_MASK, now = (nb + PAGE_SIZE - 1) & PAGE_MASK;
    if (now > old) { if ((now - old) / PAGE_SIZE + BRK_RESERVE_FRAMES > pmm_free_frames()) return (int)t->brk; }
    else if (now < old) vmm_unmap(t->pd, now, (old - now) / PAGE_SIZE, 1);
    if (!t->lazy_lo) t->lazy_lo = t->brk_start;
    t->brk = nb;
    return (int)t->brk;
}

static int fault_code(u32 vec) {
    switch (vec) { case 0: return 128 + 8; case 6: return 128 + 4; case 17: return 128 + 7; default: return 128 + 11; }
}
void proc_fault(Regs *r, u32 addr) {
    Task *t = task_current();
    char l1[64], l2[64];
    static const char *nm[20] = { "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range", "Invalid opcode",
        "Device n/a", "Double fault", "Coproc overrun", "Invalid TSS", "Segment absent", "Stack fault", "General protection",
        "Page fault", "Reserved", "x87 FPU", "Alignment", "Machine check", "SIMD" };
    u32 vec = REGS_VEC(r);
    const char *what = vec < 20 ? nm[vec] : "Exception";
    klog("proc %d (%s): %s, pc=%p addr=%p err=%x -> killed\n", t->pid, t->name, what, REGS_PC(r), addr, REGS_ERR(r));
    ksnprintf(l1, sizeof l1, "%s (pid %d) was terminated: %s", t->name, t->pid, what);
    ksnprintf(l2, sizeof l2, "PC=%08x  addr=%08x", REGS_PC(r), addr);
    preempt_disable();
    dialog_msg("Program error", l1, l2);
    desktop_invalidate();
    preempt_enable();
    task_exit(fault_code(vec));
}
