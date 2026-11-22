#include "proc/task.h"
#include "mem/heap.h"
#include "mem/vmm.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/errno.h"
#include "arch/io.h"
#include "arch/timer.h"
#include "arch/context.h"
#include "fs/vfs.h"
#include "fs/persist.h"
#include "gui/window.h"

static Task tasks[MAXTASK];
static Task *cur;
static int next_pid = 1;
static volatile int need_resched;
volatile int sys_idle;
static u32 idle_ticks, idle_prev, load_pct;

Task *task_current(void) { return cur; }
Task *task_get(int i) { return (i >= 0 && i < MAXTASK && tasks[i].state != T_UNUSED) ? &tasks[i] : NULL; }
Task *task_by_pid(int pid) {
    for (int i = 0; i < MAXTASK; i++) if (tasks[i].state != T_UNUSED && tasks[i].pid == pid) return &tasks[i];
    return NULL;
}
int task_is_user(const Task *t) { return t && t->pd != 0; }

int task_cwd_in_use(const struct VNode *n) {
    for (int i = 0; i < MAXTASK; i++) {
        Task *t = &tasks[i];
        if (t->state == T_UNUSED) continue;
        if (t->cwd == n) return 1;
        for (int k = 0; k < MAXFD; k++) if (t->fds[k] && t->fds[k]->vn == n) return 1;
    }
    return 0;
}

void task_wake(Task *t) {
    u32 f = irq_save();
    if (t->state == T_SLEEP && t->ev_wait) { t->ev_wait = 0; t->state = T_READY; }
    irq_restore(f);
}

void task_wake_obj(void *obj) {
    u32 f = irq_save();
    for (int i = 0; i < MAXTASK; i++) {
        Task *t = &tasks[i];
        if (t->state == T_SLEEP && t->ev_wait && t->wait_obj == obj) { t->ev_wait = 0; t->state = T_READY; }
    }
    irq_restore(f);
}

void task_init(void) {
    memset(tasks, 0, sizeof tasks);
    tasks[0].pid = 0; tasks[0].state = T_RUNNING;
    strlcpy(tasks[0].name, "kernel/gui", sizeof tasks[0].name);
    cur = &tasks[0];
}

void preempt_disable(void) { u32 f = irq_save(); cur->pcnt++; irq_restore(f); }
void preempt_enable(void)  {
    u32 f = irq_save();
    int r = (--cur->pcnt == 0 && need_resched);
    irq_restore(f);
    if (r) task_yield();
}
int preempt_count(void) { return cur ? cur->pcnt : 0; }

static void schedule(void) {
    u32 fl = irq_save();
    Task *old = cur, *next = NULL;
    u32 now = timer_ticks();
    for (int i = 0; i < MAXTASK; i++)
        if (tasks[i].state == T_SLEEP && (s32)(now - tasks[i].wake) >= 0) { tasks[i].state = T_READY; tasks[i].ev_wait = 0; }
    int start = (int)(old - tasks);
    for (int i = 1; i <= MAXTASK; i++) {
        Task *t = &tasks[(start + i) % MAXTASK];
        if (t->state == T_READY || t->state == T_RUNNING) { next = t; break; }
    }
    if (!next) next = &tasks[0];
    need_resched = 0;
    if (next != old) {
        if (old->state == T_RUNNING) old->state = T_READY;
        next->state = T_RUNNING;
        cur = next;
        if (next->pd) arch_set_kstack((u32)next->stack + STACK_SZ);
        u32 want = next->pd ? next->pd : vmm_kernel_pd();
        if (vmm_current() != want) vmm_switch(want);
        switch_context(&old->esp, next->esp);
    }
    irq_restore(fl);
}

void sched_tick(void) {
    if (!cur) return;
    cur->cpu++;
    if (cur == &tasks[0] && sys_idle) idle_ticks++;
    if (cur->pcnt) { need_resched = 1; return; }
    schedule();
}

void task_yield(void) { schedule(); }

void task_sleep_ms(u32 ms) {
    u32 fl = irq_save();
    cur->wake = timer_ticks() + (ms + 9) / 10;
    cur->state = T_SLEEP;
    irq_restore(fl);
    schedule();
}

void task_wait_on(void *obj) {
    u32 fl = irq_save();
    cur->wait_obj = obj; cur->ev_wait = 1;
    cur->wake = timer_ticks() + 0x7FFFFFFFu;
    cur->state = T_SLEEP;
    irq_restore(fl);
    schedule();
    cur->ev_wait = 0; cur->wait_obj = NULL;
}

static Task *slot_reserve(void) {
    u32 fl = irq_save();
    Task *t = NULL;
    for (int i = 1; i < MAXTASK; i++) if (tasks[i].state == T_UNUSED) { t = &tasks[i]; break; }
    if (t) { t->state = T_SLEEP; t->wake = 0x7FFFFFFFu; t->ev_wait = 0; }
    irq_restore(fl);
    return t;
}

#ifdef GOS_STACKMARK
#define STK_PAT 0xA5A5A5A5u
static u32 stk_hw_max;
static u8 *stack_alloc(void) {
    u8 *s = kmalloc(STACK_SZ);
    if (s) for (u32 i = 0; i < STACK_SZ / 4; i++) ((u32 *)s)[i] = STK_PAT;
    return s;
}
static u32 stack_used(const u8 *s) {
    u32 i = 0;
    while (i < STACK_SZ / 4 && ((const u32 *)s)[i] == STK_PAT) i++;
    return STACK_SZ - i * 4;
}
u32 task_stack_hw(void) {
    u32 m = stk_hw_max;
    for (int i = 1; i < MAXTASK; i++) if (tasks[i].state != T_UNUSED && tasks[i].stack) { u32 u = stack_used(tasks[i].stack); if (u > m) m = u; }
    return m;
}
#define STACK_NEW() stack_alloc()
#else
#define STACK_NEW() kmalloc(STACK_SZ)
#endif

static void common_init(Task *t, const char *name, u8 *stk, int parent, u16 uid, u16 gid, File *const *std) {
    memset(t->fds, 0, sizeof t->fds);
    t->stack = stk; t->cpu = 0; t->nwin = 0; t->ev_wait = 0;
    t->pcnt = 0; t->exit_code = 0; t->collected = 0; t->wait_obj = NULL;
    t->parent = parent; t->uid = uid; t->gid = gid;
    t->pid = next_pid++;
    strlcpy(t->name, name, sizeof t->name);
    t->cwd = cur ? cur->cwd : NULL;
    for (int i = 0; i < 3; i++)
        t->fds[i] = (std && std[i]) ? file_ref(std[i]) : fd_dup_std(i);
}

static void trampoline(void) {
    sti();
    cur->fn(cur->arg);
    task_exit(0);
}

int task_spawn(const char *name, void (*fn)(void *), void *arg) {
    Task *t = slot_reserve();
    if (!t) return -1;
    u8 *stk = STACK_NEW();
    if (!stk) { t->state = T_UNUSED; return -1; }
    common_init(t, name, stk, 0, 0, 0, NULL);
    t->fn = fn; t->arg = arg;
    t->pd = 0; t->brk = t->brk_start = 0; t->lazy_lo = 0;
    t->esp = arch_task_frame(stk, STACK_SZ, trampoline);
    t->state = T_READY;
    return t->pid;
}

static void user_trampoline(void) {
    ARCH_ENTER_USER(cur);
}

int task_create_user(const char *name, u32 pd, u32 entry, u32 usp, u32 brk, u32 lazy_lo,
                     int parent, u16 uid, u16 gid, File *const *std) {
    Task *t = slot_reserve();
    if (!t) return -1;
    u8 *stk = STACK_NEW();
    if (!stk) { t->state = T_UNUSED; return -1; }
    common_init(t, name, stk, parent, uid, gid, std);
    t->fn = NULL; t->arg = NULL;
    t->pd = pd; t->brk = t->brk_start = brk; t->lazy_lo = lazy_lo;
    t->user_eip = entry; t->user_esp = usp;
    t->esp = arch_task_frame(stk, STACK_SZ, user_trampoline);
    t->state = T_READY;
    return t->pid;
}

static void close_all_fds(Task *t) {
    for (int k = 0; k < MAXFD; k++) if (t->fds[k]) { File *f = t->fds[k]; t->fds[k] = NULL; fd_release(f); }
}

static void make_zombie(Task *t, int code) {
    fs_sync_abort(t);
    if (t->pd) uwin_destroy_owned(t);
    close_all_fds(t);
    u32 f = irq_save();
    t->exit_code = code & 0xFF;
    t->state = T_ZOMBIE;
    Task *p = t->parent ? task_by_pid(t->parent) : NULL;
    irq_restore(f);
    if (p) task_wake_obj(p);
}

void task_exit(int code) {
    preempt_disable();
    make_zombie(cur, code);
    schedule();
    for (;;) hlt();
}

int task_kill(int pid) {
    if (pid <= 0) return -EPERM;
    for (int i = 1; i < MAXTASK; i++) {
        Task *t = &tasks[i];
        if (t->state == T_UNUSED || t->state == T_ZOMBIE || t->pid != pid) continue;
        if (!t->pd) return -EPERM;
        if (cur->uid != 0 && cur->uid != t->uid) return -EPERM;
        if (t == cur) task_exit(137);
        preempt_disable();
        make_zombie(t, 137);
        preempt_enable();
        return 0;
    }
    return -ESRCH;
}

int task_wait(int pid, int *code, int flags) {
    Task *me = cur;
    preempt_disable();
    int ret;
    for (;;) {
        int have = 0; ret = 0;
        for (int i = 1; i < MAXTASK; i++) {
            Task *t = &tasks[i];
            if (t->state == T_UNUSED || t->parent != me->pid || t->collected) continue;
            if (pid > 0 && t->pid != pid) continue;
            have = 1;
            if (t->state == T_ZOMBIE) { *code = t->exit_code; t->collected = 1; ret = t->pid; break; }
        }
        if (ret) break;
        if (!have) { ret = -ECHILD; break; }
        if (flags & WNOHANG) { ret = 0; break; }
        task_wait_on(me);
    }
    preempt_enable();
    return ret;
}

void task_reap(void) {
    for (int i = 1; i < MAXTASK; i++) {
        Task *t = &tasks[i];
        if (t->state != T_ZOMBIE || t == cur) continue;
        if (t->stack) {
            close_all_fds(t);
            if (t->pd) { vmm_destroy_space(t->pd); t->pd = 0; }
#ifdef GOS_STACKMARK
            { u32 u = stack_used(t->stack); if (u > stk_hw_max) stk_hw_max = u; }
#endif
            kfree(t->stack); t->stack = NULL;
            t->cwd = NULL;
        }
        Task *p = t->parent ? task_by_pid(t->parent) : NULL;
        int parent_alive = p && p->state != T_ZOMBIE;
        if (t->collected || !parent_alive) t->state = T_UNUSED;
    }
}

void cpu_load_sample(void) {
    u32 d = idle_ticks - idle_prev;
    idle_prev = idle_ticks;
    if (d > HZ) d = HZ;
    load_pct = 100 - d * 100 / HZ;
}
u32 cpu_load_pct(void) { return load_pct; }
