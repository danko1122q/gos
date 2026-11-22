#include "shell.h"
#include "shell_identity.h"

#define MAXTOK  64
#define MAXARG  16
#define MAXSTG  6
enum { T_WORD = 0, T_PIPE, T_SEMI, T_OUT, T_APP, T_IN, T_BG };

typedef struct { char *s; int op; } Tok;
typedef struct { char *argv[MAXARG]; int argc; char *in, *out; int append; } Stage;

const char *shell_getenv(Shell *sh, const char *k) {
    for (int i = 0; i < sh->nenv; i++) if (!strcmp(sh->env[i].k, k)) return sh->env[i].v;
    return NULL;
}
void shell_setenv(Shell *sh, const char *k, const char *v) {
    for (int i = 0; i < sh->nenv; i++)
        if (!strcmp(sh->env[i].k, k)) { strlcpy(sh->env[i].v, v, sizeof sh->env[0].v); return; }
    if (sh->nenv >= 16) return;
    strlcpy(sh->env[sh->nenv].k, k, sizeof sh->env[0].k);
    strlcpy(sh->env[sh->nenv].v, v, sizeof sh->env[0].v);
    sh->nenv++;
}

void shell_init(Shell *sh) {
    memset(sh, 0, sizeof *sh);
    sh->uid = gos_getuid();
    const char *home = shell_home_for_uid(sh->uid);
    if (gos_chdir(home) < 0) gos_chdir("/");
    shell_setenv(sh, "USER", shell_user_for_uid(sh->uid)); shell_setenv(sh, "HOME", home);
    shell_setenv(sh, "SHELL", "/bin/sh"); shell_setenv(sh, "PATH", "/bin");
    shell_setenv(sh, "HOSTNAME", "gos");
}

void shell_err(Shell *sh, const char *fmt, ...) {
    char b[200];
    va_list ap; va_start(ap, fmt);
    int n = kvsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof b) n = sizeof b - 1;
    if (sh->ui_write) sh->ui_write(sh->ui, b, n);
}

void shell_prompt(Shell *sh, char *buf, int max) {
    char p[128], *home = (char *)shell_getenv(sh, "HOME");
    if (gos_getcwd(p, sizeof p) < 0) strcpy(p, "/");
    const char *user = shell_getenv(sh, "USER");
    if (home && home[1] && !strncmp(p, home, strlen(home)) && (p[strlen(home)] == 0 || p[strlen(home)] == '/')) {
        char t[128]; t[0] = '~'; strlcpy(t + 1, p + strlen(home), sizeof t - 1);
        strlcpy(p, t, sizeof p);
    }
    ksnprintf(buf, max, "%s@gos:%s%c ", user ? user : "user", p, shell_prompt_mark_for_uid(sh->uid));
}

static int expand_var(Shell *sh, const char **pp, char *o, int *op, int omax) {
    const char *p = *pp;
    char name[20]; int n = 0; const char *val = NULL; char tmp[128];
    if (*p == '?') { ksnprintf(tmp, sizeof tmp, "%d", sh->status); val = tmp; p++; }
    else {
        int br = (*p == '{'); if (br) p++;
        while ((isalpha(*p) || *p == '_' || (n && isdigit(*p))) && n < 19) name[n++] = *p++;
        name[n] = 0;
        if (br && *p == '}') p++;
        if (!n) { if (*op < omax - 1) o[(*op)++] = '$'; *pp = p; return 0; }
        if (!strcmp(name, "PWD")) { if (gos_getcwd(tmp, sizeof tmp) < 0) strcpy(tmp, "/"); val = tmp; }
        else val = shell_getenv(sh, name);
    }
    if (val) while (*val && *op < omax - 1) o[(*op)++] = *val++;
    *pp = p;
    return 0;
}

static int tokenize(Shell *sh, const char *s, Tok *tk, char *pool, int psz) {
    int nt = 0, pp = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        if (*s == '#') break;
        if (nt >= MAXTOK - 1) return -1;
        if (*s == '|') { tk[nt].s = NULL; tk[nt++].op = T_PIPE; s++; continue; }
        if (*s == ';') { tk[nt].s = NULL; tk[nt++].op = T_SEMI; s++; continue; }
        if (*s == '<') { tk[nt].s = NULL; tk[nt++].op = T_IN; s++; continue; }
        if (*s == '&') { tk[nt].s = NULL; tk[nt++].op = T_BG; s++; continue; }
        if (*s == '>') {
            s++; int ap = (*s == '>'); if (ap) s++;
            tk[nt].s = NULL; tk[nt++].op = ap ? T_APP : T_OUT; continue;
        }
        char *w = pool + pp; int wl = 0, any = 0;
        while (*s && *s != ' ' && *s != '\t' && *s != '|' && *s != ';' && *s != '<' && *s != '>' && *s != '&') {
            if (pp + wl >= psz - 2) return -1;
            if (*s == '\'') { any = 1; s++; while (*s && *s != '\'') { if (pp + wl < psz - 2) w[wl++] = *s; s++; } if (*s) s++; }
            else if (*s == '"') {
                any = 1; s++;
                while (*s && *s != '"') {
                    if (pp + wl >= psz - 2) return -1;
                    if (*s == '$') { s++; int o = pp + wl; expand_var(sh, &s, pool, &o, psz - 2); wl = o - pp; }
                    else if (*s == '\\' && (s[1] == '"' || s[1] == '\\' || s[1] == '$')) { w[wl++] = s[1]; s += 2; }
                    else w[wl++] = *s++;
                }
                if (*s) s++;
            }
            else if (*s == '$') { any = 1; s++; int o = pp + wl; expand_var(sh, &s, pool, &o, psz - 2); wl = o - pp; }
            else if (*s == '~' && wl == 0 && (s[1] == 0 || s[1] == '/' || s[1] == ' ')) {
                const char *h = shell_getenv(sh, "HOME"); any = 1; s++;
                while (h && *h && pp + wl < psz - 2) w[wl++] = *h++;
            }
            else if (*s == '\\' && s[1]) { any = 1; w[wl++] = s[1]; s += 2; }
            else { any = 1; w[wl++] = *s++; }
        }
        w[wl] = 0;
        if (any) { tk[nt].s = w; tk[nt++].op = T_WORD; pp += wl + 1; }
    }
    return nt;
}

static int is_elf(const char *path) {
    int fd = gos_open(path, O_RDONLY);
    if (fd < 0) return 0;
    u8 m[4]; int r = gos_read(fd, m, 4);
    gos_close(fd);
    return r == 4 && !memcmp(m, "\x7f" "ELF", 4);
}

static int is_detached_prog(const char *path) {
    static const char *const gui[] = { "terminal", "files", "editor", "sysmon" };
    const char *b = strrchr(path, '/'); b = b ? b + 1 : path;
    for (int i = 0; i < ARRAY_LEN(gui); i++) if (!strcmp(b, gui[i])) return 1;
    return 0;
}

static int run_elf(Shell *sh, const char *path, int argc, char **argv, IO *io) {
    const char *av[MAXARG + 1];
    for (int i = 0; i < argc && i < MAXARG; i++) av[i] = argv[i];
    av[argc < MAXARG ? argc : MAXARG] = NULL;

    if (io->bg || is_detached_prog(path)) {
        int nul = gos_open("/dev/null", O_RDWR);
        int map[3] = { nul >= 0 ? nul : GOS_FD_INHERIT, GOS_FD_INHERIT, GOS_FD_INHERIT };
        int pid = gos_spawn_ex(path, av, map, SPAWN_DETACH);
        if (nul >= 0) gos_close(nul);
        if (pid < 0) { shell_err(sh, "%s: %s\n", argv[0], gos_strerror(pid)); return 126; }
        buf_printf(io->out, "[%d] %s started\n", pid, argv[0]);
        return 0;
    }

    int piped_in = io->in && io->in->p;
    int interactive = io->tty && !piped_in && sh->ui_poll;
    int have_in = piped_in || interactive;
    int outp[2] = { -1, -1 }, inp[2] = { -1, -1 }, nul = -1, r;
    if ((r = gos_pipe(outp, PIPE_RD_NONBLOCK)) < 0) { shell_err(sh, "%s: pipe: %s\n", argv[0], gos_strerror(r)); return 126; }
    if (have_in) { if ((r = gos_pipe(inp, PIPE_WR_NONBLOCK)) < 0) { gos_close(outp[0]); gos_close(outp[1]); shell_err(sh, "%s: pipe: %s\n", argv[0], gos_strerror(r)); return 126; } }
    else nul = gos_open("/dev/null", O_RDONLY);
    int map[3] = { have_in ? inp[0] : (nul >= 0 ? nul : GOS_FD_INHERIT), outp[1], outp[1] };
    int pid = gos_spawn_ex(path, av, map, 0);
    gos_close(outp[1]);
    if (have_in) gos_close(inp[0]);
    if (nul >= 0) gos_close(nul);
    if (pid < 0) {
        shell_err(sh, "%s: %s\n", argv[0], gos_strerror(pid));
        gos_close(outp[0]); if (have_in) gos_close(inp[1]);
        return 126;
    }

    int in_off = 0, in_open = have_in, status = 0, killed = 0;
    char tmp[256];
    for (;;) {
        for (;;) {
            int n = gos_read(outp[0], tmp, sizeof tmp);
            if (n <= 0) break;
            if (io->tty) { if (sh->ui_write) sh->ui_write(sh->ui, tmp, n); } else buf_put(io->out, tmp, n);
        }
        if (in_open && piped_in) {
            if (in_off < io->in->len) {
                int c = io->in->len - in_off; if (c > 512) c = 512;
                int n = gos_write(inp[1], io->in->p + in_off, (u32)c);
                if (n > 0) in_off += n; else if (n == -EPIPE) in_off = io->in->len;
            }
            if (in_off >= io->in->len) { gos_close(inp[1]); in_open = 0; }
        }
        int w = gos_wait(pid, &status, WNOHANG);
        if (w == pid || w < 0) {
            if (w < 0) status = 126;
            for (int n; (n = gos_read(outp[0], tmp, sizeof tmp)) > 0; ) {
                if (io->tty) { if (sh->ui_write) sh->ui_write(sh->ui, tmp, n); } else buf_put(io->out, tmp, n);
            }
            break;
        }
        if (sh->ui_poll) {
            char line[128];
            int n = sh->ui_poll(sh->ui, line, sizeof line, 12);
            if (n == SH_POLL_INT) { if (!killed) { gos_kill(pid); killed = 1; } }
            else if (n == SH_POLL_EOF) { if (in_open && interactive) { gos_close(inp[1]); in_open = 0; } }
            else if (n > 0 && in_open && interactive) gos_write(inp[1], line, (u32)n);
        } else gos_sleep_ms(12);
    }
    gos_close(outp[0]);
    if (in_open) gos_close(inp[1]);
    return status;
}

static int run_cmd(Shell *sh, Stage *st, IO *io) {
    char **argv = st->argv; int argc = st->argc;
    const Cmd *c = cmd_find(argv[0]);
    if (c) return c->fn(sh, argc, argv, io);
    char *eq = strchr(argv[0], '=');
    if (eq && eq != argv[0]) { *eq = 0; shell_setenv(sh, argv[0], eq + 1); return 0; }
    char path[96];
    if (strchr(argv[0], '/')) strlcpy(path, argv[0], sizeof path);
    else ksnprintf(path, sizeof path, "/bin/%s", argv[0]);
    if (!strncmp(path, "/bin/", 5) && (c = cmd_find(path + 5))) return c->fn(sh, argc, argv, io);
    GStat gs;
    int r = gos_stat(path, &gs);
    if (r == 0) {
        if (gs.type == VN_DIR) { shell_err(sh, "%s: Is a directory\n", argv[0]); return 126; }
        if (gs.type == VN_FILE && is_elf(path)) return run_elf(sh, path, argc, argv, io);
        if (gs.type == VN_FILE) {
            if (sh->depth < 4) { sh->depth++; int rc = shell_run_script(sh, path); sh->depth--; return rc; }
            shell_err(sh, "%s: nested too deeply\n", argv[0]); return 1;
        }
    }
    shell_err(sh, "%s: command not found\n", argv[0]);
    return 127;
}

static void run_pipeline(Shell *sh, Stage *stg, int ns, int bg) {
    Buf prev; buf_init(&prev);
    for (int k = 0; k < ns; k++) {
        Stage *st = &stg[k];
        if (st->argc == 0) { shell_err(sh, "syntax error: empty command\n"); sh->status = 2; buf_free(&prev); return; }
        if (st->in) {
            char *d; u32 l;
            int r = gfs_read(st->in, &d, &l);
            if (r) { shell_err(sh, "%s: %s\n", st->in, gos_strerror(r)); sh->status = 1; buf_free(&prev); return; }
            buf_free(&prev); buf_put(&prev, d, (int)l); kfree(d);
        }
        Buf out; buf_init(&out);
        IO io = { &prev, &out, (k == ns - 1) && !st->out, bg && k == ns - 1 };
        sh->status = run_cmd(sh, st, &io);
        buf_free(&prev);
        prev = out;
    }
    Stage *last = &stg[ns - 1];
    if (last->out) {
        int r = gfs_write(last->out, prev.p ? prev.p : "", (u32)prev.len, last->append);
        if (r) { shell_err(sh, "%s: %s\n", last->out, gos_strerror(r)); sh->status = 1; }
    } else if (prev.len && sh->ui_write) sh->ui_write(sh->ui, prev.p, prev.len);
    buf_free(&prev);
}

void shell_exec(Shell *sh, const char *line) {
    Tok tk[MAXTOK]; char pool[1024];
    int n = tokenize(sh, line, tk, pool, sizeof pool);
    if (n < 0) { shell_err(sh, "line too long\n"); sh->status = 2; return; }
    int i = 0;
    while (i < n) {
        Stage stg[MAXSTG]; int ns = 0, bad = 0, bg = 0;
        memset(stg, 0, sizeof stg);
        for (; i < n && tk[i].op != T_SEMI; i++) {
            Stage *st = &stg[ns];
            switch (tk[i].op) {
            case T_WORD: if (st->argc < MAXARG - 1) st->argv[st->argc++] = tk[i].s; break;
            case T_PIPE: if (++ns >= MAXSTG) { bad = 1; ns = MAXSTG - 1; } break;
            case T_BG: bg = 1; break;
            case T_OUT: case T_APP: case T_IN:
                if (i + 1 < n && tk[i + 1].op == T_WORD) {
                    if (tk[i].op == T_IN) st->in = tk[i + 1].s; else { st->out = tk[i + 1].s; st->append = tk[i].op == T_APP; }
                    i++;
                } else bad = 1;
                break;
            }
        }
        if (i < n) i++;
        ns++;
        if (bad) { shell_err(sh, "syntax error\n"); sh->status = 2; continue; }
        if (ns == 1 && stg[0].argc == 0) continue;
        run_pipeline(sh, stg, ns, bg);
    }
}

static int run_script_text(Shell *sh, char *text) {
    char *p = text;
    while (*p) {
        char *e = strchr(p, '\n');
        if (e) *e = 0;
        if (*p && *p != '#') shell_exec(sh, p);
        if (!e) break;
        p = e + 1;
    }
    return sh->status;
}

int shell_run_script(Shell *sh, const char *path) {
    char *d; u32 l;
    int r = gfs_read(path, &d, &l);
    if (r) { shell_err(sh, "%s: %s\n", path, gos_strerror(r)); return 127; }
    run_script_text(sh, d);
    kfree(d);
    return sh->status;
}

static int starts(const char *s, const char *p, int pl) { return !strncmp(s, p, pl); }

int shell_complete(Shell *sh, char *line, int *len, int max, Buf *list) {
    (void)sh;
    int ws = *len;
    while (ws > 0 && line[ws - 1] != ' ') ws--;
    char word[96]; int wl = *len - ws;
    if (wl >= (int)sizeof word) return 0;
    memcpy(word, line + ws, wl); word[wl] = 0;
    int first = 1;
    for (int i = 0; i < ws; i++) if (line[i] != ' ') first = 0;
    static char cand[64][GOS_NAME_MAX + 2]; int nc = 0;
    char *slash = strrchr(word, '/');
    const char *base = slash ? slash + 1 : word; int bl = (int)strlen(base);
    if (first && !slash) {
        for (int i = 0; i < cmd_count() && nc < 64; i++)
            if (starts(cmd_at(i)->name, base, bl)) strlcpy(cand[nc++], cmd_at(i)->name, GOS_NAME_MAX);
        GDirEnt e;
        for (int i = 0; nc < 64 && gos_readdir("/bin", i, &e) > 0; i++)
            if (starts(e.name, base, bl)) strlcpy(cand[nc++], e.name, GOS_NAME_MAX);
    } else {
        char dir[96]; int dl = slash ? (int)(slash - word) + 1 : 0;
        memcpy(dir, word, dl); dir[dl] = 0;
        GDirEnt e;
        for (int i = 0; nc < 64 && gos_readdir(dl ? dir : ".", i, &e) > 0; i++)
            if (starts(e.name, base, bl)) {
                strlcpy(cand[nc], e.name, GOS_NAME_MAX);
                if (e.type == VN_DIR) strcat(cand[nc], "/");
                nc++;
            }
    }
    if (!nc) return 0;
    char common[GOS_NAME_MAX + 2]; strlcpy(common, cand[0], sizeof common);
    for (int i = 1; i < nc; i++) {
        int k = 0; while (common[k] && common[k] == cand[i][k]) k++;
        common[k] = 0;
    }
    int cl = (int)strlen(common);
    if (cl > bl) {
        int add = cl - bl;
        if (*len + add < max - 1) { memcpy(line + *len, common + bl, add); *len += add; line[*len] = 0; }
    }
    if (nc == 1) {
        if (cand[0][strlen(cand[0]) - 1] != '/' && *len < max - 2) { line[(*len)++] = ' '; line[*len] = 0; }
    } else if (list) {
        for (int i = 0; i < nc; i++) { buf_puts(list, cand[i]); buf_puts(list, "  "); }
        buf_putc(list, '\n');
    }
    return nc;
}
