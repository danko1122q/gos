#include "fs/devfs.h"
#include "abi.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/klog.h"
#include "mem/heap.h"
#include "arch/timer.h"
#include "arch/arch.h"
#include "proc/task.h"

typedef int (*Gen)(char *buf, int max);

static int g_version(char *b, int m) { return ksnprintf(b, m, "GOS version " GOS_VERSION " (" GOS_ARCH_NAME ", hobby unix-like) #1\n"); }
static int g_uptime(char *b, int m) {
    u32 t = timer_ticks();
    return ksnprintf(b, m, "%u.%02u\n", t / 100, t % 100);
}
static int g_meminfo(char *b, int m) {
    return ksnprintf(b, m, "MemTotal:  %8u kB\nHeapTotal: %8u kB\nHeapUsed:  %8u kB\nHeapFree:  %8u kB\n",
        mem_total_kb(), heap_total() / 1024, heap_used() / 1024, (heap_total() - heap_used()) / 1024);
}
static int g_cpuinfo(char *b, int m) {
    int n = arch_cpuinfo(b, m);
    if (n < 0) n = 0;
    if (n < m) n += ksnprintf(b + n, m - n, "load      : %u%%\n", cpu_load_pct());
    return n;
}
static int g_tasks(char *b, int m) {
    static const char st[] = "?RRSZ";
    int n = ksnprintf(b, m, "PID PPID  UID STAT   CPU NAME\n");
    for (int i = 0; i < MAXTASK; i++) {
        Task *t = task_get(i);
        if (!t || n >= m - 40) continue;
        n += ksnprintf(b + n, m - n, "%3d %4d %4d %c    %5u %s\n", t->pid, t->parent, t->uid, st[t->state], t->cpu, t->name);
    }
    return n;
}
static int g_kmsg(char *b, int m) { return klog_read(b, m - 1); }
static int g_mounts(char *b, int m) {
    return ksnprintf(b, m, "ramfs  /      rw\ndevfs  /dev   rw\nprocfs /proc  ro\nbuiltin /bin  ro\n");
}

static int proc_read(VNode *n, u32 off, void *buf, u32 len) {
    Gen g = (Gen)n->data;
    char *tmp = kmalloc(8192);
    if (!tmp) return -1;
    int l = g(tmp, 8192);
    int r = 0;
    if (l > 0 && off < (u32)l) { r = (int)MIN(len, (u32)l - off); memcpy(buf, tmp + off, r); }
    kfree(tmp);
    return r;
}
static const VOps o_proc = { proc_read, NULL };

void procfs_init(VNode *p) {
    vfs_add_dev(p, "version", &o_proc, g_version);
    vfs_add_dev(p, "uptime",  &o_proc, g_uptime);
    vfs_add_dev(p, "meminfo", &o_proc, g_meminfo);
    vfs_add_dev(p, "cpuinfo", &o_proc, g_cpuinfo);
    vfs_add_dev(p, "tasks",   &o_proc, g_tasks);
    vfs_add_dev(p, "kmsg",    &o_proc, g_kmsg);
    vfs_add_dev(p, "mounts",  &o_proc, g_mounts);
}
