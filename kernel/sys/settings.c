#include "sys/settings.h"
#include "gui/desktop.h"
#include "fs/vfs.h"
#include "proc/task.h"
#include "mem/heap.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/errno.h"

#define CONF "/etc/desktop.conf"

volatile int g_set[SET_COUNT] = { 0, 1, 1, 1, 1, 1, 5 };

static const char *const key[SET_COUNT] = { "color", "pattern", "mouse", "dblclick", "keydelay", "keyrate", "volume" };
static const int maxv[SET_COUNT] = { GOS_DESK_COLORS - 1, GOS_DESK_PATTERNS - 1, 3, 2, 3, 3, 7 };

static const int dbl_ms[3]  = { 800, 500, 300 };
static const int delay_t[4] = { 80, 40, 25, 15 };
static const int rate_t[4]  = { 6, 3, 2, 1 };
static const int mul_n[4]   = { 1, 1, 3, 2 };
static const int mul_d[4]   = { 2, 1, 2, 1 };

int  set_dblclick_ms(void)     { int i = g_set[SET_DBLCLICK]; return dbl_ms[i < 0 || i > 2 ? 1 : i]; }
int  set_key_delay_ticks(void) { return delay_t[g_set[SET_KEY_DELAY] & 3]; }
int  set_key_rate_ticks(void)  { return rate_t[g_set[SET_KEY_RATE] & 3]; }
void set_mouse_scale(int *num, int *den) { int i = g_set[SET_MOUSE_SPEED] & 3; *num = mul_n[i]; *den = mul_d[i]; }

static void save(void) {
    char t[160]; int n = 0;
    for (int i = 0; i < SET_COUNT; i++) {
        if (i == SET_DESK_COLOR) n += ksnprintf(t + n, sizeof t - (u32)n, "color=%s\n", desktop_color_name(g_set[i]));
        else n += ksnprintf(t + n, sizeof t - (u32)n, "%s=%d\n", key[i], g_set[i]);
    }
    Task *tk = task_current(); u16 ou = 0, og = 0;
    if (tk) { ou = tk->uid; og = tk->gid; tk->uid = 0; tk->gid = 0; }
    vfs_write_file(NULL, CONF, t, (u32)n, 0);
    if (tk) { tk->uid = ou; tk->gid = og; }
}

static int parse_int(const char *s, int *out) {
    if (*s < '0' || *s > '9') return -1;
    int v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    *out = v; return 0;
}

void settings_init(void) {
    char *d; u32 l; (void)l;
    if (vfs_read_file(NULL, CONF, &d, &l)) return;
    for (char *p = d; *p; ) {
        char *e = strchr(p, '\n');
        if (e) *e = 0;
        char *eq = strchr(p, '=');
        if (eq) {
            *eq = 0;
            for (int i = 0; i < SET_COUNT; i++) {
                if (strcmp(p, key[i])) continue;
                int v;
                if (i == SET_DESK_COLOR && !(eq[1] >= '0' && eq[1] <= '9')) v = desktop_color_by_name(eq + 1);
                else if (parse_int(eq + 1, &v)) v = -1;
                if (v >= 0 && v <= maxv[i]) g_set[i] = v;
            }
        }
        if (!e) break;
        p = e + 1;
    }
    kfree(d);
}

int settings_set(int id, int v) {
    if (id < 0 || id >= SET_COUNT || v < 0 || v > maxv[id]) return -EINVAL;
    if (g_set[id] == v) return 0;
    g_set[id] = v;
    if (id == SET_DESK_COLOR) desktop_apply_color(v);
    if (id == SET_DESK_COLOR || id == SET_DESK_PATTERN) desktop_invalidate();
    save();
    return 0;
}
