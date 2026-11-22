#include "proc/exec.h"
#include "fs/vfs.h"
#include "gui/dialog.h"
#include "gui/desktop.h"
#include "mem/heap.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/errno.h"

static int launch_as(const char *prog, const char *arg, int uid) {
    char path[48]; ksnprintf(path, sizeof path, "/bin/%s", prog);
    char *argv[2]; int argc = 0;
    argv[argc++] = (char *)prog;
    if (arg && *arg) argv[argc++] = (char *)arg;
    int r = uid < 0 ? proc_spawn(path, argc, argv) : proc_spawn_ex(path, argc, argv, 0, (u16)uid, (u16)uid, NULL);
    if (r < 0) {
        char l[64]; ksnprintf(l, sizeof l, "%s failed to start (error %d)", prog, -r);
        dialog_msg("Can't open program", l, r == -EAGAIN ? "Process table full." : "");
        desktop_invalidate();
    }
    return r;
}
int launch_prog(const char *prog, const char *arg) { return launch_as(prog, arg, -1); }

static int app_launch_as(const char *spec, int uid) {
    while (*spec == ' ') spec++;
    if (strncmp(spec, "app:", 4)) return -1;
    spec += 4;
    char name[16]; int n = 0;
    while (*spec && *spec != ' ' && *spec != '\n' && n < 15) name[n++] = *spec++;
    name[n] = 0;
    while (*spec == ' ') spec++;
    char arg[128]; int a = 0;
    while (*spec && *spec != '\n' && a < 127) arg[a++] = *spec++;
    arg[a] = 0;
    if (!strcmp(name, "terminal")) return launch_as("terminal", arg, uid);
    if (!strcmp(name, "files"))    return launch_as("files", arg[0] ? arg : "/home/user", uid);
    if (!strcmp(name, "edit"))     return launch_as("editor", arg, uid);
    if (!strcmp(name, "sysmon"))   return launch_as("sysmon", NULL, uid);
    return launch_as(name, arg, uid);
}
int app_launch(const char *spec) { return app_launch_as(spec, -1); }

void app_open_path(const char *p) {
    VNode *n = vfs_lookup(NULL, p);
    if (!n) return;
    if (n->type == VN_DIR) { launch_prog("files", p); return; }
    const char *e = strrchr(n->name, '.');
    if (e && !strcmp(e, ".app")) {
        char *d; u32 l;
        if (!vfs_read_file(NULL, p, &d, &l)) {
            if (n->uid == 0 && !strncmp(d, "root:", 5)) app_launch_as(d + 5, 0);
            else app_launch(d);
            kfree(d);
        }
        return;
    }
    if (e && (!strcmp(e, ".bmp") || !strcmp(e, ".BMP"))) { launch_prog("paint", p); return; }
    if (e && !strcmp(e, ".sh")) {
        char cmd[160]; ksnprintf(cmd, sizeof cmd, "sh %s", p);
        launch_prog("terminal", cmd);
        return;
    }
    if (n->type == VN_FILE && n->size >= 4 && n->data && !memcmp(n->data, "\x7f" "ELF", 4)) {
        char *argv[1] = { (char *)n->name };
        proc_spawn(p, 1, argv);
        return;
    }
    launch_prog("editor", p);
}
