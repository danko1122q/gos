#include "shell.h"

#define OUT(...) buf_printf(io->out, __VA_ARGS__)

static const char *mon_names[12] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
static const char *day_names[7] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };

static void fmt_mode(const GStat *st, char *m) {
    m[0] = st->type == VN_DIR ? 'd' : st->type == VN_DEV ? 'c' : '-';
    for (int i = 0; i < 9; i++) m[1 + i] = (st->mode & (0400 >> i)) ? "rwx"[i % 3] : '-';
    if (st->mode & 01000) m[9] = (st->mode & 1) ? 't' : 'T';
    m[10] = 0;
}
static void fmt_time(u32 e, char *o, int max) {
    DateTime t; epoch_to_dt(e, &t);
    ksnprintf(o, max, "%04d-%02d-%02d %02d:%02d", t.year, t.mon, t.day, t.hour, t.min);
}

static char *get_text(Shell *sh, IO *io, const char *file, u32 *len) {
    if (file) {
        char *d; u32 l; int r = gfs_read(file, &d, &l);
        if (r) { shell_err(sh, "%s: %s\n", file, gos_strerror(r)); return NULL; }
        if (len) *len = l;
        return d;
    }
    int l = io->in && io->in->p ? io->in->len : 0;
    char *d = kmalloc((size_t)l + 1);
    if (!d) return NULL;
    if (l) memcpy(d, io->in->p, l);
    d[l] = 0; if (len) *len = (u32)l;
    return d;
}

static int is_dir(const char *p) { return gfs_isdir(p); }
static const char *base_of(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }
static void child_path(const char *dir, const char *name, char *out, int max) {
    if (!strcmp(dir, "/")) ksnprintf(out, max, "/%s", name);
    else ksnprintf(out, max, "%s/%s", dir, name);
}

static int c_pwd(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv; char p[128]; if (gos_getcwd(p, sizeof p) < 0) strcpy(p, "/"); OUT("%s\n", p); return 0;
}
static int c_cd(Shell *sh, int argc, char **argv, IO *io) {
    (void)io;
    const char *t = argc > 1 ? argv[1] : shell_getenv(sh, "HOME");
    GStat st;
    int r = gos_stat(t, &st);
    if (r < 0) { shell_err(sh, "cd: %s: No such file or directory\n", t); return 1; }
    if (st.type != VN_DIR) { shell_err(sh, "cd: %s: Not a directory\n", t); return 1; }
    r = gos_chdir(t);
    if (r < 0) { shell_err(sh, "cd: %s: %s\n", t, gos_strerror(r)); return 1; }
    return 0;
}

static void ls_long(Buf *o, const char *path, const char *name, int isdir) {
    GStat st; if (gos_stat(path, &st) < 0) return;
    char m[11], t[20]; fmt_mode(&st, m); fmt_time(st.mtime, t, sizeof t);
    buf_printf(o, "%s %4d %4d %7u %s %s%s\n", m, st.uid, st.gid, st.size, t, name, isdir ? "/" : "");
}
static int c_ls(Shell *sh, int argc, char **argv, IO *io) {
    int lf = 0, all = 0, np = 0, rc = 0; char *paths[8];
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            for (char *f = argv[i] + 1; *f; f++) { if (*f == 'l') lf = 1; else if (*f == 'a') all = 1; }
        } else if (np < 8) paths[np++] = argv[i];
    }
    if (!np) paths[np++] = ".";
    for (int k = 0; k < np; k++) {
        GStat st;
        if (gos_stat(paths[k], &st) < 0) { shell_err(sh, "ls: %s: No such file or directory\n", paths[k]); rc = 1; continue; }
        if (np > 1) OUT("%s%s:\n", k ? "\n" : "", paths[k]);
        if (st.type != VN_DIR) { if (lf) ls_long(io->out, paths[k], base_of(paths[k]), 0); else OUT("%s\n", base_of(paths[k])); continue; }
        int col = 0; GDirEnt e;
        for (int i = 0; gos_readdir(paths[k], i, &e) > 0; i++) {
            if (e.name[0] == '.' && !all) continue;
            if (lf) { char p[200]; child_path(paths[k], e.name, p, sizeof p); ls_long(io->out, p, e.name, e.type == VN_DIR); continue; }
            if (!io->tty) { OUT("%s\n", e.name); continue; }
            int l = (int)strlen(e.name) + (e.type == VN_DIR) + 2;
            if (col + l > 68) { buf_putc(io->out, '\n'); col = 0; }
            OUT("%s%s  ", e.name, e.type == VN_DIR ? "/" : ""); col += l;
        }
        if (!lf && io->tty && col) buf_putc(io->out, '\n');
    }
    return rc;
}

static int c_cat(Shell *sh, int argc, char **argv, IO *io) {
    if (argc == 1) { if (io->in && io->in->p) buf_put(io->out, io->in->p, io->in->len); return 0; }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        u32 l; char *d = get_text(sh, io, argv[i], &l);
        if (!d) { rc = 1; continue; }
        buf_put(io->out, d, (int)l); kfree(d);
    }
    return rc;
}

static int c_echo(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; int nl = 1, i = 1, esc = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "-n")) nl = 0; else if (!strcmp(argv[i], "-e")) esc = 1; else break;
    }
    for (; i < argc; i++) {
        for (const char *p = argv[i]; *p; p++) {
            if (esc && *p == '\\' && p[1]) { p++; buf_putc(io->out, *p == 'n' ? '\n' : *p == 't' ? '\t' : *p); }
            else buf_putc(io->out, *p);
        }
        if (i < argc - 1) buf_putc(io->out, ' ');
    }
    if (nl) buf_putc(io->out, '\n');
    return 0;
}

static int mkdir_p(const char *path) {
    char abs[256]; path_join(path, abs, sizeof abs);
    char cur[256]; cur[0] = 0;
    for (const char *p = abs; *p; ) {
        const char *e = strchr(p + 1, '/');
        int l = e ? (int)(e - p) : (int)strlen(p);
        memcpy(cur + strlen(cur), p, l); cur[strlen(cur) + l] = 0;
        if (!gfs_exists(cur)) { int r = gos_mkdir(cur); if (r) return r; }
        p += l; if (!*p) break;
    }
    return 0;
}
static int c_mkdir(Shell *sh, int argc, char **argv, IO *io) {
    (void)io; int p = 0, rc = 0;
    if (argc < 2) { shell_err(sh, "usage: mkdir [-p] dir...\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) { p = 1; continue; }
        int r = p ? mkdir_p(argv[i]) : gos_mkdir(argv[i]);
        if (r) { shell_err(sh, "mkdir: %s: %s\n", argv[i], gos_strerror(r)); rc = 1; }
    }
    return rc;
}
static int c_rmdir(Shell *sh, int argc, char **argv, IO *io) {
    (void)io; int rc = 0;
    for (int i = 1; i < argc; i++) { int r = gos_rmdir(argv[i]); if (r) { shell_err(sh, "rmdir: %s: %s\n", argv[i], gos_strerror(r)); rc = 1; } }
    return rc;
}
static int c_touch(Shell *sh, int argc, char **argv, IO *io) {
    (void)io; int rc = 0;
    for (int i = 1; i < argc; i++) {
        int r = gfs_write(argv[i], "", 0, 1);
        if (r) { shell_err(sh, "touch: %s: %s\n", argv[i], gos_strerror(r)); rc = 1; }
    }
    return rc;
}

static int rm_tree(const char *path) {
    GStat st; int r = gos_stat(path, &st);
    if (r < 0) return r;
    if (st.type == VN_DIR) {
        GDirEnt e;
        while (gos_readdir(path, 0, &e) > 0) {
            char c[200]; child_path(path, e.name, c, sizeof c);
            r = rm_tree(c); if (r) return r;
        }
        return gos_rmdir(path);
    }
    return gos_unlink(path);
}
static int c_rm(Shell *sh, int argc, char **argv, IO *io) {
    (void)io; int rec = 0, force = 0, rc = 0, files = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) { for (char *f = argv[i] + 1; *f; f++) { if (*f == 'r' || *f == 'R') rec = 1; if (*f == 'f') force = 1; } continue; }
        files++;
        GStat st;
        if (gos_stat(argv[i], &st) < 0) { if (!force) { shell_err(sh, "rm: %s: No such file or directory\n", argv[i]); rc = 1; } continue; }
        if (st.type == VN_DIR && !rec) { shell_err(sh, "rm: %s: Is a directory (use -r)\n", argv[i]); rc = 1; continue; }
        char abs[256]; path_join(argv[i], abs, sizeof abs);
        int r = rm_tree(abs);
        if (r) { shell_err(sh, "rm: %s: %s\n", argv[i], gos_strerror(r)); rc = 1; }
    }
    if (!files) { shell_err(sh, "usage: rm [-rf] file...\n"); return 1; }
    return rc;
}

static int copy_tree(const char *src, const char *dst, int depth) {
    GStat st;
    int r = gos_stat(src, &st);
    if (r < 0) return r;
    if (st.type == VN_DIR) {
        if (depth > 8) return -ENAMETOOLONG;
        if (!gfs_exists(dst)) { r = gos_mkdir(dst); if (r) return r; }
        GDirEnt e;
        for (int i = 0; gos_readdir(src, i, &e) > 0; i++) {
            char a[200], b[200];
            child_path(src, e.name, a, sizeof a); child_path(dst, e.name, b, sizeof b);
            r = copy_tree(a, b, depth + 1); if (r) return r;
        }
        return 0;
    }
    char *d; u32 l; r = gfs_read(src, &d, &l);
    if (r) return r;
    r = gfs_write(dst, d, l, 0);
    kfree(d);
    return r;
}
static int c_cp(Shell *sh, int argc, char **argv, IO *io) {
    (void)io; int rec = 0, na = 0; char *a[3];
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "-R")) rec = 1; else if (na < 3) a[na++] = argv[i];
    }
    if (na != 2) { shell_err(sh, "usage: cp [-r] source dest\n"); return 1; }
    if (is_dir(a[0]) && !rec) { shell_err(sh, "cp: %s is a directory (use -r)\n", a[0]); return 1; }
    char dst[160], src[160];
    path_join(a[0], src, sizeof src);
    if (is_dir(a[1])) { char t[160]; path_join(a[1], t, sizeof t); child_path(t, base_of(a[0]), dst, sizeof dst); }
    else path_join(a[1], dst, sizeof dst);
    int r = copy_tree(src, dst, 0);
    if (r) { shell_err(sh, "cp: %s\n", gos_strerror(r)); return 1; }
    return 0;
}
static int c_mv(Shell *sh, int argc, char **argv, IO *io) {
    (void)io;
    if (argc != 3) { shell_err(sh, "usage: mv source dest\n"); return 1; }
    char dst[160];
    if (is_dir(argv[2])) { char t[160]; path_join(argv[2], t, sizeof t); child_path(t, base_of(argv[1]), dst, sizeof dst); }
    else strlcpy(dst, argv[2], sizeof dst);
    int r = gos_rename(argv[1], dst);
    if (r) { shell_err(sh, "mv: %s\n", gos_strerror(r)); return 1; }
    return 0;
}
static int c_chmod(Shell *sh, int argc, char **argv, IO *io) {
    (void)io;
    if (argc != 3) { shell_err(sh, "usage: chmod 755 file\n"); return 1; }
    int m = 0; for (const char *p = argv[1]; *p >= '0' && *p <= '7'; p++) m = m * 8 + (*p - '0');
    int r = gos_chmod(argv[2], m & 01777);
    if (r) { shell_err(sh, "chmod: %s: %s\n", argv[2], gos_strerror(r)); return 1; }
    return 0;
}
static int parse_id(const char *s) {
    if (!strcmp(s, "root")) return 0;
    if (!strcmp(s, "user")) return GOS_UID_USER;
    return atoi(s);
}
static int c_chown(Shell *sh, int argc, char **argv, IO *io) {
    (void)io;
    if (argc != 3) { shell_err(sh, "usage: chown uid[:gid] file   (uid: number, root, user)\n"); return 1; }
    char own[32]; strlcpy(own, argv[1], sizeof own);
    char *colon = strchr(own, ':'); int gid;
    if (colon) { *colon = 0; gid = parse_id(colon + 1); } else gid = -1;
    int uid = parse_id(own);
    if (gid < 0) { GStat st; gid = gos_stat(argv[2], &st) == 0 ? st.gid : uid; }
    int r = gos_chown(argv[2], uid, gid);
    if (r) { shell_err(sh, "chown: %s: %s\n", argv[2], gos_strerror(r)); return 1; }
    return 0;
}
static int c_stat(Shell *sh, int argc, char **argv, IO *io) {
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        GStat st; int r = gos_stat(argv[i], &st);
        if (r) { shell_err(sh, "stat: %s: %s\n", argv[i], gos_strerror(r)); rc = 1; continue; }
        char m[11], t[20]; fmt_mode(&st, m); fmt_time(st.mtime, t, sizeof t);
        OUT("  File: %s\n  Size: %u   Type: %s   Inode: %u\n  Mode: %s (%04o)  Uid: %d  Gid: %d\n  Modify: %s\n", argv[i], st.size,
            st.type == VN_DIR ? "directory" : st.type == VN_DEV ? "special" : "regular file", st.ino, m, st.mode, st.uid, st.gid, t);
    }
    return rc;
}

static void tree_rec(Buf *o, const char *dir, char *prefix, int depth) {
    GDirEnt e, nx;
    for (int i = 0; gos_readdir(dir, i, &e) > 0; i++) {
        int last = gos_readdir(dir, i + 1, &nx) <= 0;
        buf_printf(o, "%s%s %s%s\n", prefix, last ? "`--" : "|--", e.name, e.type == VN_DIR ? "/" : "");
        if (e.type == VN_DIR && depth < 5 && strcmp(e.name, "proc") && strcmp(e.name, "dev")) {
            int pl = (int)strlen(prefix);
            if (pl < 60) {
                char c[200]; child_path(dir, e.name, c, sizeof c);
                strcat(prefix, last ? "    " : "|   "); tree_rec(o, c, prefix, depth + 1); prefix[pl] = 0;
            }
        }
    }
}
static int c_tree(Shell *sh, int argc, char **argv, IO *io) {
    char abs[200]; path_join(argc > 1 ? argv[1] : ".", abs, sizeof abs);
    if (!gfs_isdir(abs)) { shell_err(sh, "tree: directory not found\n"); return 1; }
    char prefix[80] = "";
    OUT("%s\n", argc > 1 ? argv[1] : "."); tree_rec(io->out, abs, prefix, 0);
    return 0;
}

static int opt_n(int argc, char **argv, int def, int *fi) {
    int n = def; *fi = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) { n = atoi(argv[++i]); *fi = i + 1; }
        else if (argv[i][0] == '-' && isdigit(argv[i][1])) { n = atoi(argv[i] + 1); *fi = i + 1; }
        else break;
    }
    return n;
}
static int c_head(Shell *sh, int argc, char **argv, IO *io) {
    int fi, n = opt_n(argc, argv, 10, &fi);
    char *d = get_text(sh, io, fi < argc ? argv[fi] : NULL, NULL); if (!d) return 1;
    char *p = d;
    for (int i = 0; i < n && *p; i++) { char *e = strchr(p, '\n'); if (!e) { buf_puts(io->out, p); buf_putc(io->out, '\n'); break; } buf_put(io->out, p, (int)(e - p) + 1); p = e + 1; }
    kfree(d); return 0;
}
static int c_tail(Shell *sh, int argc, char **argv, IO *io) {
    int fi, n = opt_n(argc, argv, 10, &fi);
    char *d = get_text(sh, io, fi < argc ? argv[fi] : NULL, NULL); if (!d) return 1;
    int l = (int)strlen(d); if (l && d[l - 1] == '\n') l--;
    int cnt = 0, i = l;
    while (i > 0) { if (d[i - 1] == '\n' && ++cnt == n) break; i--; }
    buf_puts(io->out, d + i); if (strlen(d) && d[strlen(d) - 1] != '\n') buf_putc(io->out, '\n');
    kfree(d); return 0;
}
static int c_wc(Shell *sh, int argc, char **argv, IO *io) {
    int fl = 0, fw = 0, fc = 0, fi = 1;
    for (; fi < argc && argv[fi][0] == '-'; fi++) for (char *f = argv[fi] + 1; *f; f++) { if (*f == 'l') fl = 1; if (*f == 'w') fw = 1; if (*f == 'c') fc = 1; }
    if (!fl && !fw && !fc) fl = fw = fc = 1;
    u32 len; char *d = get_text(sh, io, fi < argc ? argv[fi] : NULL, &len); if (!d) return 1;
    int lines = 0, words = 0, inw = 0;
    for (u32 i = 0; i < len; i++) { if (d[i] == '\n') lines++; if (isspace(d[i])) inw = 0; else if (!inw) { inw = 1; words++; } }
    if (fl) OUT("%7d", lines);
    if (fw) OUT("%7d", words);
    if (fc) OUT("%7u", len);
    if (fi < argc) OUT(" %s", argv[fi]);
    buf_putc(io->out, '\n'); kfree(d); return 0;
}
static int ci_find(const char *h, const char *n, int ci) {
    if (!ci) return strstr(h, n) != NULL;
    int nl = (int)strlen(n);
    for (; *h; h++) { int k = 0; while (k < nl && h[k] && tolower(h[k]) == tolower(n[k])) k++; if (k == nl) return 1; }
    return nl == 0;
}
static int c_grep(Shell *sh, int argc, char **argv, IO *io) {
    int inv = 0, ci = 0, num = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) for (char *f = argv[i] + 1; *f; f++) { if (*f == 'v') inv = 1; if (*f == 'i') ci = 1; if (*f == 'n') num = 1; }
    if (i >= argc) { shell_err(sh, "usage: grep [-ivn] pola [file...]\n"); return 2; }
    const char *pat = argv[i++]; int nf = argc - i, found = 0;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        const char *fn = nf ? argv[i + k] : NULL;
        char *d = get_text(sh, io, fn, NULL); if (!d) continue;
        int ln = 0;
        for (char *p = d; *p; ) {
            char *e = strchr(p, '\n'); if (e) *e = 0;
            ln++;
            if (ci_find(p, pat, ci) != inv) {
                found = 1;
                if (nf > 1) OUT("%s:", fn);
                if (num) OUT("%d:", ln);
                OUT("%s\n", p);
            }
            if (!e) break;
            p = e + 1;
        }
        kfree(d);
    }
    return found ? 0 : 1;
}
static int c_sort(Shell *sh, int argc, char **argv, IO *io) {
    int rev = 0, numr = 0, fi = 1;
    for (; fi < argc && argv[fi][0] == '-'; fi++) for (char *f = argv[fi] + 1; *f; f++) { if (*f == 'r') rev = 1; if (*f == 'n') numr = 1; }
    char *d = get_text(sh, io, fi < argc ? argv[fi] : NULL, NULL); if (!d) return 1;
    char *ls[512]; int n = 0;
    for (char *p = d; *p && n < 512; ) { ls[n++] = p; char *e = strchr(p, '\n'); if (!e) break; *e = 0; p = e + 1; }
    for (int gap = n / 2; gap > 0; gap /= 2)
        for (int i = gap; i < n; i++) {
            char *t = ls[i]; int j = i;
            for (; j >= gap; j -= gap) {
                int c = numr ? atoi(ls[j - gap]) - atoi(t) : strcmp(ls[j - gap], t);
                if (rev) c = -c;
                if (c <= 0) break;
                ls[j] = ls[j - gap];
            }
            ls[j] = t;
        }
    for (int i = 0; i < n; i++) OUT("%s\n", ls[i]);
    kfree(d); return 0;
}
static int c_seq(Shell *sh, int argc, char **argv, IO *io) {
    int a = 1, b;
    if (argc == 2) b = atoi(argv[1]); else if (argc == 3) { a = atoi(argv[1]); b = atoi(argv[2]); }
    else { shell_err(sh, "usage: seq [first] last\n"); return 1; }
    for (int i = a; i <= b && i - a < 5000; i++) OUT("%d\n", i);
    return 0;
}
static int c_tee(Shell *sh, int argc, char **argv, IO *io) {
    if (argc < 2) { shell_err(sh, "usage: tee file\n"); return 1; }
    if (io->in && io->in->p) { buf_put(io->out, io->in->p, io->in->len); gfs_write(argv[1], io->in->p, (u32)io->in->len, 0); }
    return 0;
}
static int c_xxd(Shell *sh, int argc, char **argv, IO *io) {
    u32 len; char *d = get_text(sh, io, argc > 1 ? argv[1] : NULL, &len); if (!d) return 1;
    if (len > 2048) len = 2048;
    for (u32 o = 0; o < len; o += 16) {
        OUT("%08x: ", o);
        for (u32 i = 0; i < 16; i++) { if (o + i < len) OUT("%02x ", (u8)d[o + i]); else OUT("   "); }
        OUT(" |");
        for (u32 i = 0; i < 16 && o + i < len; i++) { u8 c = (u8)d[o + i]; buf_putc(io->out, c >= 32 && c < 127 ? (char)c : '.'); }
        OUT("|\n");
    }
    kfree(d); return 0;
}

static int c_date(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv;
    DateTime t; epoch_to_dt(gos_time(), &t);
    OUT("%s %s %d %02d:%02d:%02d %d\n", day_names[t.wday], mon_names[t.mon - 1], t.day, t.hour, t.min, t.sec, t.year);
    return 0;
}
static int c_uptime(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv;
    GSysInfo si; gos_sysinfo(&si);
    u32 s = si.hz ? si.ticks / si.hz : 0; DateTime t; epoch_to_dt(gos_time(), &t);
    int np = 0; GProcInfo pi;
    for (int i = 0; i < GOS_MAXTASK; i++) if (gos_procinfo(i, &pi) == 0 && pi.state != 4) np++;
    OUT(" %02d:%02d:%02d up %u:%02u:%02u, CPU load %u%%, %d processes\n", t.hour, t.min, t.sec, s / 3600, s / 60 % 60, s % 60, si.cpu_load, np);
    return 0;
}
static int c_uname(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh;
    if (argc > 1 && !strcmp(argv[1], "-a")) OUT("GOS gos " GOS_VERSION " #1 i386 GOS/unix-like (ring 3 userland)\n");
    else if (argc > 1 && !strcmp(argv[1], "-r")) OUT(GOS_VERSION "\n");
    else if (argc > 1 && !strcmp(argv[1], "-m")) OUT("i386\n");
    else if (argc > 1 && !strcmp(argv[1], "-n")) OUT("gos\n");
    else OUT("GOS\n");
    return 0;
}
static int c_whoami(Shell *sh, int argc, char **argv, IO *io) { (void)argc; (void)argv; OUT("%s\n", sh->uid == 0 ? "root" : "user"); return 0; }
static int c_id(Shell *sh, int argc, char **argv, IO *io) {
    (void)argc; (void)argv;
    int u = gos_getuid(); const char *n = sh->uid == 0 ? "root" : "user";
    OUT("uid=%d(%s) gid=%d(%s)\n", u, n, u, n);
    return 0;
}
static int c_hostname(Shell *sh, int argc, char **argv, IO *io) { (void)sh; (void)argc; (void)argv; OUT("gos\n"); return 0; }

static int c_ps(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv;
    static const char st[] = "?RRSZ";
    OUT("  PID  PPID  UID STAT RING    CPU    MEM NAME\n");
    for (int i = 0; i < GOS_MAXTASK; i++) {
        GProcInfo pi; if (gos_procinfo(i, &pi) < 0) continue;
        OUT("%5d %5d %4d  %c     %d %6u %4uK %s", pi.pid, pi.ppid, pi.uid, st[pi.state > 4 ? 0 : pi.state], pi.user ? 3 : 0, pi.cpu, pi.mem_kb, pi.name);
        if (pi.state == 4) OUT(" (exit %d)", pi.exit_code);
        buf_putc(io->out, '\n');
    }
    return 0;
}
static int c_kill(Shell *sh, int argc, char **argv, IO *io) {
    (void)io; int rc = 0;
    if (argc < 2) { shell_err(sh, "usage: kill [-9] pid...\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        int r = gos_kill(atoi(argv[i]));
        if (r) { shell_err(sh, "kill: (%s) %s\n", argv[i], gos_strerror(r)); rc = 1; }
    }
    return rc;
}
static const char *daemon_names = "ticker counter burn (or any other program in /bin)";
static int c_spawn(Shell *sh, int argc, char **argv, IO *io) {
    if (argc < 2) { shell_err(sh, "usage: spawn <%s>\n", daemon_names); return 1; }
    char path[48]; ksnprintf(path, sizeof path, "/bin/%s", argv[1]);
    const char *av[2] = { argv[1], NULL };
    int pid = gos_spawn(path, av);
    if (pid == -ENOENT) { shell_err(sh, "spawn: program not found. Examples: %s\n", daemon_names); return 1; }
    if (pid < 0) { shell_err(sh, "spawn: %s\n", gos_strerror(pid)); return 1; }
    OUT("[%d] %s started\n", pid, argv[1]);
    return 0;
}
static int c_free(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv;
    GSysInfo si; gos_sysinfo(&si);
    u32 t = si.heap_total / 1024, u = si.heap_used / 1024;
    OUT("            total      used      free\n");
    OUT("Heap:   %8u  %8u  %8u KB (heap kernel)\n", t, u, t - u);
    OUT("Frame: %8u  %8u  %8u KB (PMM, 4 KB/frame)\n", si.pmm_total * 4, (si.pmm_total - si.pmm_free) * 4, si.pmm_free * 4);
    OUT("RAM:    %8u KB (detected by BIOS)\n", si.ram_kb);
    return 0;
}
static int c_df(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv;
    GSysInfo si; gos_sysinfo(&si);
    OUT("Filesystem  Files  Dirs  Bytes\nramfs      %6u %5u  %u\n", si.vfs_files, si.vfs_dirs, si.vfs_bytes);
    if (si.ata) OUT("Disk: ATA active, snapshot %u bytes, %s\n", si.fs_sync_bytes, si.fs_dirty ? "unsaved changes" : "saved");
    else OUT("Disk: none (data is lost on reboot)\n");
    return 0;
}
static int c_dmesg(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv;
    char *b = kmalloc(GOS_KLOG_MAX);
    if (!b) return 1;
    int n = gos_klog(b, GOS_KLOG_MAX);
    if (n > 0) buf_put(io->out, b, n);
    kfree(b);
    return 0;
}
static int c_env(Shell *sh, int argc, char **argv, IO *io) {
    (void)argc; (void)argv;
    for (int i = 0; i < sh->nenv; i++) OUT("%s=%s\n", sh->env[i].k, sh->env[i].v);
    return 0;
}
static int c_export(Shell *sh, int argc, char **argv, IO *io) {
    if (argc == 1) return c_env(sh, argc, argv, io);
    for (int i = 1; i < argc; i++) { char *e = strchr(argv[i], '='); if (e) { *e = 0; shell_setenv(sh, argv[i], e + 1); } }
    return 0;
}
static int c_sync(Shell *sh, int argc, char **argv, IO *io) {
    (void)argc; (void)argv;
    GSysInfo si; gos_sysinfo(&si);
    if (!si.ata) { shell_err(sh, "sync: no disk\n"); return 1; }
    int r = gos_sync();
    if (r) { shell_err(sh, "sync: failed (%s)\n", gos_strerror(r)); return 1; }
    gos_sysinfo(&si);
    OUT("disk synced (%u bytes)\n", si.fs_sync_bytes);
    return 0;
}
static int c_reboot(Shell *sh, int argc, char **argv, IO *io) { (void)sh; (void)argc; (void)argv; (void)io; gos_reboot(); return 0; }
static int c_poweroff(Shell *sh, int argc, char **argv, IO *io) { (void)sh; (void)argc; (void)argv; (void)io; gos_poweroff(); return 0; }
static int c_clear(Shell *sh, int argc, char **argv, IO *io) { (void)argc; (void)argv; (void)io; if (sh->ui_clear) sh->ui_clear(sh->ui); return 0; }
static int c_exit(Shell *sh, int argc, char **argv, IO *io) { (void)argc; (void)argv; (void)io; if (sh->ui_exit) sh->ui_exit(sh->ui); return 0; }
static int c_true(Shell *sh, int argc, char **argv, IO *io) { (void)sh; (void)argc; (void)argv; (void)io; return 0; }
static int c_false(Shell *sh, int argc, char **argv, IO *io) { (void)sh; (void)argc; (void)argv; (void)io; return 1; }
static int c_sh(Shell *sh, int argc, char **argv, IO *io) {
    (void)io;
    if (argc < 2) { shell_err(sh, "usage: sh script\n"); return 1; }
    if (sh->depth >= 4) { shell_err(sh, "sh: nested too deeply\n"); return 1; }
    sh->depth++; int r = shell_run_script(sh, argv[1]); sh->depth--; return r;
}
static int c_which(Shell *sh, int argc, char **argv, IO *io) {
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        char p[64]; ksnprintf(p, sizeof p, "/bin/%s", argv[i]);
        if (cmd_find(argv[i]) || gfs_exists(p)) OUT("/bin/%s\n", argv[i]);
        else { shell_err(sh, "which: %s not found\n", argv[i]); rc = 1; }
    }
    return rc;
}
static int c_mount(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv; char *d; u32 l;
    if (!gfs_read("/proc/mounts", &d, &l)) { buf_put(io->out, d, (int)l); kfree(d); }
    return 0;
}

static int run_gui(Shell *sh, const char *prog, const char *arg) {
    char path[48]; ksnprintf(path, sizeof path, "/bin/%s", prog);
    const char *av[3] = { prog, arg, NULL };
    if (!arg) av[1] = NULL;
    int pid = gos_spawn(path, av);
    if (pid < 0) { shell_err(sh, "%s: %s\n", prog, gos_strerror(pid)); return 1; }
    return 0;
}
static int c_edit(Shell *sh, int argc, char **argv, IO *io) {
    (void)io; char p[128];
    if (argc > 1) { path_join(argv[1], p, sizeof p); return run_gui(sh, "editor", p); }
    return run_gui(sh, "editor", NULL);
}
static int c_files(Shell *sh, int argc, char **argv, IO *io) {
    (void)io; char p[128]; path_join(argc > 1 ? argv[1] : ".", p, sizeof p);
    return run_gui(sh, "files", p);
}
static int c_top(Shell *sh, int argc, char **argv, IO *io) { (void)argc; (void)argv; (void)io; return run_gui(sh, "sysmon", NULL); }
static int c_term(Shell *sh, int argc, char **argv, IO *io) { (void)argc; (void)argv; (void)io; return run_gui(sh, "terminal", NULL); }

static int c_help(Shell *sh, int argc, char **argv, IO *io);

static const Cmd cmds[] = {
    { "help", c_help, "list commands" },
    { "ls", c_ls, "[-la] [path] list directory contents" }, { "cd", c_cd, "[dir] change directory" },
    { "pwd", c_pwd, "print working directory" },            { "cat", c_cat, "[file...] print file contents" },
    { "echo", c_echo, "[-ne] text" },                       { "mkdir", c_mkdir, "[-p] dir..." },
    { "rmdir", c_rmdir, "empty dir" },                      { "rm", c_rm, "[-rf] file..." },
    { "touch", c_touch, "file... create/update" },          { "cp", c_cp, "[-r] source dest" },
    { "mv", c_mv, "source dest (move/rename)" },            { "chmod", c_chmod, "755 file" },
    { "stat", c_stat, "file... inode info" },               { "tree", c_tree, "[dir] directory tree" },
    { "head", c_head, "[-n N] [file] first lines" },        { "tail", c_tail, "[-n N] [file] last lines" },
    { "wc", c_wc, "[-lwc] [file] count" },                  { "grep", c_grep, "[-ivn] pattern [file] search text" },
    { "sort", c_sort, "[-rn] [file] sort" },                { "seq", c_seq, "[first] last" },
    { "tee", c_tee, "file  (stdin -> file + stdout)" },     { "xxd", c_xxd, "[file] hexdump" },
    { "date", c_date, "date & time" },                      { "uptime", c_uptime, "uptime & load" },
    { "uname", c_uname, "[-a]" },                           { "whoami", c_whoami, "" },
    { "id", c_id, "" },                                     { "hostname", c_hostname, "" },
    { "ps", c_ps, "process list (ring 0/3, memory)" },      { "kill", c_kill, "[-9] pid" },
    { "spawn", c_spawn, "program  run a /bin program in the background (same as: program &)" },
    { "chown", c_chown, "uid[:gid] file  change owner (root only)" },
    { "free", c_free, "memory usage (kernel heap, PMM frames)" },
    { "df", c_df, "filesystem & disk usage" },
    { "dmesg", c_dmesg, "kernel log" },                     { "env", c_env, "environment variables" },
    { "export", c_export, "VAR=value" },                    { "sync", c_sync, "save to disk" },
    { "mount", c_mount, "" },                               { "which", c_which, "command" },
    { "sh", c_sh, "script  run a shell script" },          { "true", c_true, "" }, { "false", c_false, "" },
    { "clear", c_clear, "clear the screen" },               { "exit", c_exit, "close the terminal" },
    { "reboot", c_reboot, "restart" },                      { "poweroff", c_poweroff, "power off" },
    { "shutdown", c_poweroff, "power off" },
    { "edit", c_edit, "[file] text editor (GUI)" },         { "files", c_files, "[dir] file manager (GUI)" },
    { "top", c_top, "system monitor (GUI)" },               { "term", c_term, "new terminal (GUI)" },
};

int cmd_count(void) { return ARRAY_LEN(cmds); }
const Cmd *cmd_at(int i) { return &cmds[i]; }
const Cmd *cmd_find(const char *n) { for (int i = 0; i < ARRAY_LEN(cmds); i++) if (!strcmp(cmds[i].name, n)) return &cmds[i]; return NULL; }

static int c_help(Shell *sh, int argc, char **argv, IO *io) {
    (void)sh; (void)argc; (void)argv;
    OUT("GOS commands (pipes |, redirects > >> <, $VAR, ; supported):\n");
    for (int i = 1; i < ARRAY_LEN(cmds); i++) OUT(" %-8s %s\n", cmds[i].name, cmds[i].help);
    OUT("Other ring 3 programs in /bin run as separate processes.\n");
    OUT("Tab = complete, Up/Down arrows = history, Ctrl+L = clear, PgUp/PgDn = scroll.\n");
    return 0;
}
