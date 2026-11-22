#ifndef GOS_SYS_WAIT_H
#define GOS_SYS_WAIT_H
#include <sys/types.h>
#define WNOHANG 1
#define WIFEXITED(status) (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WIFSIGNALED(status) (((status) & 0x7f) != 0)
#define WTERMSIG(status) ((status) & 0x7f)
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
#endif
