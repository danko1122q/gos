#include "gos.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#define FF_READ  0x01u
#define FF_WRITE 0x02u
#define FF_EOF   0x08u
#define FF_ERROR 0x10u
#define FF_STD   0x20u

struct __gos_FILE {
    int fd;
    unsigned int flags;
    int mode;
    char *buffer;
    size_t capacity;
    size_t read_pos, read_len, write_len;
    int last_op, pushback, has_pushback;
    struct __gos_FILE *next;
    char internal[BUFSIZ];
};

static FILE in_file = { .fd = 0, .flags = FF_READ | FF_STD, .mode = _IONBF };
static FILE out_file = { .fd = 1, .flags = FF_WRITE | FF_STD, .mode = _IONBF };
static FILE err_file = { .fd = 2, .flags = FF_WRITE | FF_STD, .mode = _IONBF };
FILE *stdin = &in_file;
FILE *stdout = &out_file;
FILE *stderr = &err_file;
static FILE *stream_list = &in_file;
static int streams_ready;

static void init_streams(void) {
    if (streams_ready) return;
    in_file.buffer = in_file.internal; in_file.capacity = 1;
    out_file.buffer = out_file.internal; out_file.capacity = 0;
    err_file.buffer = err_file.internal; err_file.capacity = 0;
    in_file.next = &out_file; out_file.next = &err_file; err_file.next = NULL;
    streams_ready = 1;
}
static void register_stream(FILE *f) {
    init_streams();
    f->next = stream_list; stream_list = f;
}
static void unregister_stream(FILE *f) {
    FILE **p = &stream_list;
    while (*p && *p != f) p = &(*p)->next;
    if (*p) *p = f->next;
}
static int flush_output(FILE *f) {
    size_t done = 0;
    while (done < f->write_len) {
        ssize_t n = write(f->fd, f->buffer + done, f->write_len - done);
        if (n < 0) { f->flags |= FF_ERROR; return -1; }
        if (!n) { errno = EIO; f->flags |= FF_ERROR; return -1; }
        done += (size_t)n;
    }
    f->write_len = 0;
    return 0;
}
static int stream_write(FILE *f, const unsigned char *src, size_t n) {
    size_t done = 0;
    if (!f || !(f->flags & FF_WRITE)) { errno = EBADF; if (f) f->flags |= FF_ERROR; return -1; }
    if (f->last_op == 1) {
        size_t unread = f->read_len - f->read_pos + (size_t)!!f->has_pushback;
        if (unread && lseek(f->fd, -(off_t)unread, SEEK_CUR) < 0) { f->flags |= FF_ERROR; return -1; }
        f->read_pos = f->read_len = 0; f->has_pushback = 0;
    }
    if (f->mode == _IONBF || !f->buffer || !f->capacity) {
        while (done < n) {
            ssize_t r = write(f->fd, src + done, n - done);
            if (r < 0) { f->flags |= FF_ERROR; return done ? (int)done : -1; }
            if (!r) { errno = EIO; f->flags |= FF_ERROR; return done ? (int)done : -1; }
            done += (size_t)r;
        }
        f->last_op = 2;
        return (int)done;
    }
    while (done < n) {
        size_t room = f->capacity - f->write_len;
        if (!room && flush_output(f) < 0) return done ? (int)done : -1;
        room = f->capacity - f->write_len;
        size_t c = n - done < room ? n - done : room;
        memcpy(f->buffer + f->write_len, src + done, c);
        f->write_len += c; done += c;
        if ((f->mode == _IOLBF && memchr(src + done - c, '\n', c)) || f->write_len == f->capacity) {
            if (flush_output(f) < 0) return (int)done;
        }
    }
    f->last_op = 2;
    return (int)done;
}
static int fill_input(FILE *f) {
    if (!(f->flags & FF_READ)) { errno = EBADF; f->flags |= FF_ERROR; return -1; }
    if (f->mode == _IONBF || !f->buffer || !f->capacity) {
        unsigned char c;
        ssize_t n = read(f->fd, &c, 1);
        if (n < 0) { f->flags |= FF_ERROR; return -1; }
        if (!n) { f->flags |= FF_EOF; return 0; }
        f->internal[0] = (char)c; f->read_pos = 0; f->read_len = 1;
        return 1;
    }
    ssize_t n = read(f->fd, f->buffer, f->capacity);
    if (n < 0) { f->flags |= FF_ERROR; return -1; }
    f->read_pos = 0; f->read_len = (size_t)n;
    if (!n) { f->flags |= FF_EOF; return 0; }
    return (int)n;
}
static int stream_getc(FILE *f) {
    init_streams();
    if (!f) { errno = EINVAL; return EOF; }
    if (!(f->flags & FF_READ)) { errno = EBADF; f->flags |= FF_ERROR; return EOF; }
    if (f->last_op == 2) { if (flush_output(f) < 0) return EOF; f->read_pos = f->read_len = 0; }
    f->last_op = 1;
    if (f->has_pushback) { f->has_pushback = 0; return f->pushback; }
    if (f->read_pos >= f->read_len) {
        f->read_pos = f->read_len = 0;
        int r = fill_input(f);
        if (r <= 0) return EOF;
    }
    return (unsigned char)f->buffer[f->read_pos++];
}

FILE *fopen(const char *path, const char *mode) {
    int flags, acc, readable = 0, writable = 0;
    if (!path || !mode || !*mode) { errno = EINVAL; return NULL; }
    switch (*mode++) {
    case 'r': flags = O_RDONLY; acc = FF_READ; readable = 1; break;
    case 'w': flags = O_WRONLY | O_CREAT | O_TRUNC; acc = FF_WRITE; writable = 1; break;
    case 'a': flags = O_WRONLY | O_CREAT | O_APPEND; acc = FF_WRITE; writable = 1; break;
    default: errno = EINVAL; return NULL;
    }
    int plus = 0;
    while (*mode) {
        if (*mode == '+') plus = 1;
        else if (*mode != 'b' && *mode != 't' && *mode != 'e') { errno = EINVAL; return NULL; }
        mode++;
    }
    if (plus) { flags = (flags & ~3) | O_RDWR; acc = FF_READ | FF_WRITE; readable = writable = 1; }
    int fd = open(path, flags, 0666);
    if (fd < 0) return NULL;
    FILE *f = calloc(1, sizeof *f);
    if (!f) { close(fd); return NULL; }
    f->fd = fd; f->flags = acc; f->mode = _IOFBF;
    f->buffer = f->internal; f->capacity = sizeof f->internal;
    (void)readable; (void)writable;
    register_stream(f);
    return f;
}


FILE *fdopen(int fd, const char *mode) {
    if (fd < 0 || !mode || !*mode) { errno = EINVAL; return NULL; }
    unsigned int flags = 0;
    struct stat st;
    if (fstat(fd, &st) < 0) return NULL;
    switch (*mode++) {
    case 'r': flags |= FF_READ; break;
    case 'w': flags |= FF_WRITE; break;
    case 'a': errno = ENOTSUP; return NULL;
    default: errno = EINVAL; return NULL;
    }
    int plus = 0;
    while (*mode) {
        if (*mode == '+') plus = 1;
        else if (*mode != 'b' && *mode != 't' && *mode != 'e') { errno = EINVAL; return NULL; }
        mode++;
    }
    if (plus) flags |= FF_READ | FF_WRITE;
    FILE *f = calloc(1, sizeof *f);
    if (!f) return NULL;
    init_streams();
    f->fd = fd; f->flags = flags; f->mode = _IOFBF;
    f->buffer = f->internal; f->capacity = sizeof f->internal;
    register_stream(f);
    return f;
}

FILE *freopen(const char *path, const char *mode, FILE *f) {
    if (!f) { errno = EINVAL; return NULL; }
    int was_std = (f->flags & FF_STD) != 0;
    if (!was_std) unregister_stream(f);
    (void)fflush(f);
    if (f->fd >= 0) (void)close(f->fd);
    FILE *n = fopen(path, mode);
    if (!n) { f->flags = FF_ERROR | (was_std ? FF_STD : 0); f->fd = -1; if (!was_std) register_stream(f); return NULL; }
    unregister_stream(n);
    int fd = n->fd; unsigned int flags = n->flags; int bmode = n->mode;
    size_t cap = n->capacity;
    memcpy(f->internal, n->internal, sizeof f->internal);
    f->fd = fd; f->flags = flags | (was_std ? FF_STD : 0); f->mode = bmode;
    f->buffer = f->internal; f->capacity = cap; f->read_pos = f->read_len = f->write_len = 0;
    free(n);
    if (was_std) f->flags |= FF_STD;
    else register_stream(f);
    return f;
}

int fflush(FILE *f) {
    init_streams();
    if (!f) {
        int result = 0;
        for (FILE *p = stream_list; p; p = p->next) if ((p->flags & FF_WRITE) && flush_output(p) < 0) result = EOF;
        return result;
    }
    if (!(f->flags & FF_WRITE)) return 0;
    return flush_output(f) < 0 ? EOF : 0;
}
int fclose(FILE *f) {
    if (!f) { errno = EINVAL; return EOF; }
    int result = fflush(f);
    if (f->fd >= 0 && close(f->fd) < 0) result = EOF;
    if (f->flags & FF_STD) {
        f->fd = -1; f->flags = FF_STD; f->read_pos = f->read_len = f->write_len = 0;
        f->has_pushback = 0; f->last_op = 0; return result;
    }
    unregister_stream(f); free(f); return result;
}
size_t fread(void *ptr, size_t size, size_t count, FILE *f) {
    init_streams();
    if (!f || (!ptr && size && count)) { errno = EINVAL; return 0; }
    if (!(f->flags & FF_READ)) { errno = EBADF; f->flags |= FF_ERROR; return 0; }
    if (!size || !count) return 0;
    if (count > 0xffffffffu / size) { errno = EOVERFLOW; f->flags |= FF_ERROR; return 0; }
    size_t bytes = size * count, done = 0; unsigned char *p = ptr;
    if (f->last_op == 2) { if (fflush(f) == EOF) return 0; f->read_pos = f->read_len = 0; }
    f->last_op = 1;
    if (f->has_pushback && done < bytes) { p[done++] = (unsigned char)f->pushback; f->has_pushback = 0; }
    while (done < bytes) {
        if (f->read_pos >= f->read_len) {
            f->read_pos = f->read_len = 0;
            int r = fill_input(f); if (r <= 0) break;
        }
        size_t n = f->read_len - f->read_pos;
        if (n > bytes - done) n = bytes - done;
        memcpy(p + done, f->buffer + f->read_pos, n);
        f->read_pos += n; done += n;
    }
    return done / size;
}
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *f) {
    if (!f || (!ptr && size && count)) { errno = EINVAL; return 0; }
    if (!size || !count) return 0;
    if (count > 0xffffffffu / size) { errno = EOVERFLOW; f->flags |= FF_ERROR; return 0; }
    int n = stream_write(f, ptr, size * count);
    return n < 0 ? 0 : (size_t)n / size;
}
int fseek(FILE *f, long offset, int whence) {
    if (!f) { errno = EINVAL; return -1; }
    if (fflush(f) == EOF) return -1;
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) { errno = EINVAL; return -1; }
    if (whence == SEEK_CUR && (f->flags & FF_READ)) offset -= (long)(f->read_len - f->read_pos + (size_t)!!f->has_pushback);
    if (offset < INT_MIN || offset > INT_MAX) { errno = EOVERFLOW; return -1; }
    off_t r = lseek(f->fd, (off_t)offset, whence);
    if (r < 0) return -1;
    f->read_pos = f->read_len = f->write_len = 0; f->has_pushback = 0; f->last_op = 0; f->flags &= ~FF_EOF;
    return 0;
}
long ftell(FILE *f) {
    if (!f) { errno = EINVAL; return -1; }
    off_t r = lseek(f->fd, 0, SEEK_CUR);
    if (r < 0) return -1;
    if (f->flags & FF_READ) r -= (off_t)(f->read_len - f->read_pos + (size_t)!!f->has_pushback);
    if (f->flags & FF_WRITE) r += (off_t)f->write_len;
    return r;
}
void rewind(FILE *f) { if (f) { (void)fseek(f, 0, SEEK_SET); clearerr(f); } }
int fgetpos(FILE *f, fpos_t *p) { if (!p) { errno = EFAULT; return -1; } long n = ftell(f); if (n < 0) return -1; *p = (fpos_t)n; return 0; }
int fsetpos(FILE *f, const fpos_t *p) { if (!p) { errno = EFAULT; return -1; } return fseek(f, *p, SEEK_SET); }
int feof(FILE *f) { return f ? !!(f->flags & FF_EOF) : 0; }
int ferror(FILE *f) { return f ? !!(f->flags & FF_ERROR) : 0; }
void clearerr(FILE *f) { if (f) f->flags &= ~(FF_EOF | FF_ERROR); }
int fileno(FILE *f) { if (!f) { errno = EINVAL; return -1; } return f->fd; }
int setvbuf(FILE *f, char *buf, int mode, size_t size) {
    if (!f || mode < _IOFBF || mode > _IONBF) { errno = EINVAL; return -1; }
    if (fflush(f) == EOF) return -1;
    if (mode == _IONBF) { buf = f->internal; size = 0; }
    else if (!buf) { buf = f->internal; if (!size) size = sizeof f->internal; }
    if (mode != _IONBF && !size) { errno = EINVAL; return -1; }
    f->buffer = buf; f->capacity = mode == _IONBF ? 0 : size; f->mode = mode;
    f->read_pos = f->read_len = f->write_len = 0; f->has_pushback = 0; f->last_op = 0;
    return 0;
}
void setbuf(FILE *f, char *buf) { (void)setvbuf(f, buf, buf ? _IOFBF : _IONBF, buf ? BUFSIZ : 0); }
int fgetc(FILE *f) { return stream_getc(f); }
int ungetc(int c, FILE *f) {
    if (!f || !(f->flags & FF_READ) || c == EOF || f->has_pushback) return EOF;
    if (f->last_op == 2) {
        if (fflush(f) == EOF) return EOF;
        f->read_pos = f->read_len = 0;
    }
    f->last_op = 1;
    f->pushback = (unsigned char)c; f->has_pushback = 1; f->flags &= ~FF_EOF; return (unsigned char)c;
}
int getc(FILE *f) { return fgetc(f); }
int getchar(void) { return fgetc(stdin); }
char *fgets(char *s, int size, FILE *f) {
    if (!s || size <= 0 || !f) { errno = EINVAL; return NULL; }
    if (size == 1) { s[0] = 0; return s; }
    int i = 0, c;
    while (i < size - 1 && (c = fgetc(f)) != EOF) { s[i++] = (char)c; if (c == '\n') break; }
    if (!i) return NULL;
    s[i] = 0; return s;
}
ssize_t getdelim(char **lineptr, size_t *capacity, int delimiter, FILE *f) {
    if (!lineptr || !capacity || !f) { errno = EINVAL; return -1; }
    if (!*lineptr || !*capacity) {
        size_t initial = *capacity ? *capacity : 128;
        char *p = realloc(*lineptr, initial);
        if (!p) return -1;
        *lineptr = p; *capacity = initial;
    }
    size_t len = 0;
    for (;;) {
        int c = fgetc(f);
        if (c == EOF) {
            if (!len) return -1;
            break;
        }
        if (len + 1 >= *capacity) {
            if (*capacity > 0x7fffffffu / 2u) { errno = EOVERFLOW; return -1; }
            size_t next = *capacity * 2u;
            char *p = realloc(*lineptr, next);
            if (!p) return -1;
            *lineptr = p; *capacity = next;
        }
        (*lineptr)[len++] = (char)c;
        if (c == (unsigned char)delimiter) break;
    }
    (*lineptr)[len] = 0;
    return (ssize_t)len;
}
ssize_t getline(char **lineptr, size_t *capacity, FILE *f) { return getdelim(lineptr, capacity, '\n', f); }
int fputc(int c, FILE *f) { unsigned char ch = (unsigned char)c; return stream_write(f, &ch, 1) == 1 ? ch : EOF; }
int putc(int c, FILE *f) { return fputc(c, f); }
int putchar(int c) { return fputc(c, stdout); }
int fputs(const char *s, FILE *f) { if (!s) { errno = EINVAL; return EOF; } size_t n = strlen(s); return stream_write(f, (const unsigned char *)s, n) < 0 ? EOF : 0; }
int puts(const char *s) { if (fputs(s, stdout) == EOF || fputc('\n', stdout) == EOF) return EOF; return 0; }
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap) { return kvsnprintf(s, n, fmt, ap); }
int snprintf(char *s, size_t n, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(s, n, fmt, ap); va_end(ap); return r; }
int vsprintf(char *s, const char *fmt, va_list ap) { return vsnprintf(s, 0xffffffffu, fmt, ap); }
int sprintf(char *s, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsprintf(s, fmt, ap); va_end(ap); return r; }
int vfprintf(FILE *f, const char *fmt, va_list ap) {
    char small[256]; va_list aq; __builtin_va_copy(aq, ap);
    int n = vsnprintf(small, sizeof small, fmt, aq); va_end(aq);
    if (n < 0) return -1;
    if ((size_t)n < sizeof small) return (int)fwrite(small, 1, (size_t)n, f) == n ? n : -1;
    char *large = malloc((size_t)n + 1);
    if (!large) return -1;
    __builtin_va_copy(aq, ap); int n2 = vsnprintf(large, (size_t)n + 1, fmt, aq); va_end(aq);
    int r = n2 == n && fwrite(large, 1, (size_t)n, f) == (size_t)n ? n : -1;
    free(large); return r;
}
int fprintf(FILE *f, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfprintf(f, fmt, ap); va_end(ap); return r; }
int vprintf(const char *fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }
int printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vprintf(fmt, ap); va_end(ap); return r; }
int vdprintf(int fd, const char *fmt, va_list ap) {
    FILE temp = { .fd = fd, .flags = FF_WRITE, .mode = _IONBF };
    return vfprintf(&temp, fmt, ap);
}
int dprintf(int fd, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vdprintf(fd, fmt, ap); va_end(ap); return r; }
