#include "fs/pipe.h"
#include "proc/task.h"
#include "mem/heap.h"
#include "lib/string.h"
#include "lib/errno.h"

#define PIPE_BUF_SZ 4096

struct Pipe {
    u8  buf[PIPE_BUF_SZ];
    u32 rd, cnt;
    int rd_open, wr_open;
};

Pipe *pipe_new(void) {
    Pipe *p = kzalloc(sizeof *p);
    if (p) { p->rd_open = p->wr_open = 1; }
    return p;
}

int pipe_read(Pipe *p, void *buf, u32 n, int nonblock) {
    if (!n) return 0;
    u8 *d = buf; int r;
    preempt_disable();
    for (;;) {
        if (p->cnt) {
            u32 c = n < p->cnt ? n : p->cnt;
            for (u32 i = 0; i < c; i++) d[i] = p->buf[(p->rd + i) % PIPE_BUF_SZ];
            p->rd = (p->rd + c) % PIPE_BUF_SZ; p->cnt -= c;
            task_wake_obj(p);
            r = (int)c; break;
        }
        if (!p->wr_open) { r = 0; break; }
        if (nonblock) { r = -EAGAIN; break; }
        task_wait_on(p);
    }
    preempt_enable();
    return r;
}

int pipe_write(Pipe *p, const void *buf, u32 n, int nonblock) {
    const u8 *s = buf; u32 done = 0; int r = 0;
    preempt_disable();
    while (done < n) {
        if (!p->rd_open) { r = done ? (int)done : -EPIPE; goto out; }
        u32 room = PIPE_BUF_SZ - p->cnt;
        if (room) {
            u32 c = n - done < room ? n - done : room;
            for (u32 i = 0; i < c; i++) p->buf[(p->rd + p->cnt + i) % PIPE_BUF_SZ] = s[done + i];
            p->cnt += c; done += c;
            task_wake_obj(p);
            continue;
        }
        if (nonblock) { r = done ? (int)done : -EAGAIN; goto out; }
        task_wait_on(p);
    }
    r = (int)done;
out:
    preempt_enable();
    return r;
}

void pipe_close_end(Pipe *p, int end) {
    preempt_disable();
    if (end == 0) p->rd_open = 0; else p->wr_open = 0;
    task_wake_obj(p);
    int dead = !p->rd_open && !p->wr_open;
    preempt_enable();
    if (dead) kfree(p);
}
