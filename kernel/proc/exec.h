#ifndef PROC_EXEC_H
#define PROC_EXEC_H
#include "lib/types.h"
#include "arch/idt.h"
#include "proc/task.h"

int  proc_spawn(const char *path, int argc, char **argv);
int  proc_spawn_ex(const char *path, int argc, char **argv, int parent, u16 uid, u16 gid, const int *fdmap);
int  proc_brk(Task *t, u32 new_brk);
void proc_fault(Regs *r, u32 addr);

int  launch_prog(const char *prog, const char *arg);
int  app_launch(const char *spec);
void app_open_path(const char *abs_path);

#endif
