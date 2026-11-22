#ifndef PROC_TASK_H
#define PROC_TASK_H
#include "lib/types.h"
#include "abi.h"

#define MAXTASK GOS_MAXTASK
#define MAXFD   16
#define STACK_SZ 8192

struct VNode; struct File;
typedef enum { T_UNUSED = 0, T_READY, T_RUNNING, T_SLEEP, T_ZOMBIE } TaskState;

typedef struct Task {
    u32 esp;
    int pid, state;
    char name[16];
    u8 *stack;
    void (*fn)(void *); void *arg;
    u32 wake;
    u32 cpu;
    struct VNode *cwd;
    struct File *fds[MAXFD];
    u32 pd;
    u32 brk, brk_start;
    u32 lazy_lo;
    u32 user_eip, user_esp;
    int ev_wait;
    int nwin;
    int pcnt;
    int parent;
    int exit_code;
    u8  collected;
    void *wait_obj;
    u16 uid, gid;
} Task;

void  task_init(void);
int   task_spawn(const char *name, void (*fn)(void *), void *arg);
int   task_create_user(const char *name, u32 pd, u32 entry, u32 usp, u32 brk, u32 lazy_lo,
                       int parent, u16 uid, u16 gid, struct File *const *std);
void  task_exit(int code) __attribute__((noreturn));
int   task_kill(int pid);
int   task_wait(int pid, int *code, int flags);
void  task_yield(void);
void  task_sleep_ms(u32 ms);
#ifdef GOS_STACKMARK
u32   task_stack_hw(void);
#endif
void  task_reap(void);
void  task_wake(Task *t);
void  task_wake_obj(void *obj);
void  task_wait_on(void *obj);
Task *task_current(void);
Task *task_get(int slot);
Task *task_by_pid(int pid);
int   task_is_user(const Task *t);
int   task_cwd_in_use(const struct VNode *n);
void  sched_tick(void);
void  preempt_disable(void);
void  preempt_enable(void);
int   preempt_count(void);

extern volatile int sys_idle;
u32   cpu_load_pct(void);
void  cpu_load_sample(void);

#endif
