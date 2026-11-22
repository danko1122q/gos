#ifndef USER_SHELL_H
#define USER_SHELL_H
#include "gos.h"

typedef struct Shell Shell;
struct Shell {
    int status;
    struct { char k[16]; char v[64]; } env[16];
    int nenv, depth;
    void *ui;
    void (*ui_write)(void *ui, const char *s, int n);
    void (*ui_clear)(void *ui);
    void (*ui_exit)(void *ui);
    int  (*ui_poll)(void *ui, char *buf, int max, int wait_ms);
    int  uid;
};
enum { SH_POLL_INT = -1, SH_POLL_EOF = -2 };

typedef struct { Buf *in; Buf *out; int tty; int bg; } IO;
typedef int (*CmdFn)(Shell *sh, int argc, char **argv, IO *io);
typedef struct { const char *name; CmdFn fn; const char *help; } Cmd;

void        shell_init(Shell *sh);
void        shell_exec(Shell *sh, const char *line);
int         shell_run_script(Shell *sh, const char *path);
const char *shell_getenv(Shell *sh, const char *k);
void        shell_setenv(Shell *sh, const char *k, const char *v);
void        shell_prompt(Shell *sh, char *buf, int max);
int         shell_complete(Shell *sh, char *line, int *len, int max, Buf *list);
void        shell_err(Shell *sh, const char *fmt, ...);

int         cmd_count(void);
const Cmd  *cmd_at(int i);
const Cmd  *cmd_find(const char *name);

#endif
