#include "gos.h"
#include <sys/wait.h>
static int cvt_wait(int r) { if (r < 0) { errno = -r; return -1; } return r; }
pid_t waitpid(pid_t pid, int *status, int options) {
    int raw = 0;
    if (options & ~WNOHANG) { errno = EINVAL; return -1; }
    int r = gos_wait(pid, status ? &raw : NULL, options);
    if (r > 0 && status) *status = (raw & 0xff) << 8;
    return (pid_t)cvt_wait(r);
}
pid_t wait(int *status) { return waitpid(-1, status, 0); }
