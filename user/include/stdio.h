#ifndef GOS_STDIO_H
#define GOS_STDIO_H
#include "lib/types.h"
#include <stdarg.h>
#include <sys/types.h>
#define EOF (-1)
#define BUFSIZ 512
#define FOPEN_MAX 16
#define L_tmpnam 32
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
typedef struct __gos_FILE FILE;
typedef s32 fpos_t;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);
FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fd, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int fclose(FILE *stream);
int fflush(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fgetpos(FILE *stream, fpos_t *pos);
int fsetpos(FILE *stream, const fpos_t *pos);
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
void setbuf(FILE *stream, char *buf);
int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
int getc(FILE *stream);
int getchar(void);
char *fgets(char *s, int size, FILE *stream);
ssize_t getdelim(char **lineptr, size_t *n, int delimiter, FILE *stream);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);
int putchar(int c);
int fputs(const char *s, FILE *stream);
int puts(const char *s);
int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);
int dprintf(int fd, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int vfscanf(FILE *stream, const char *fmt, va_list ap);
int fscanf(FILE *stream, const char *fmt, ...);
int scanf(const char *fmt, ...);
int vscanf(const char *fmt, va_list ap);
int vsscanf(const char *str, const char *fmt, va_list ap);
int sscanf(const char *str, const char *fmt, ...);
int vdprintf(int fd, const char *fmt, va_list ap);
int sprintf(char *str, const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsprintf(char *str, const char *fmt, va_list ap);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
void perror(const char *prefix);
#endif
