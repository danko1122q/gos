#ifndef GOS_UNISTD_H
#define GOS_UNISTD_H
#include <sys/types.h>
#include "abi.h"
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char *path);
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int dup(int fd);
int dup2(int oldfd, int newfd);
int pipe(int pipefd[2]);
int pipe2(int pipefd[2], int flags);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
gid_t getgid(void);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
int sync(void);
int rmdir(const char *path);
int mkdir(const char *path, mode_t mode);
int chmod(const char *path, mode_t mode);
int chown(const char *path, uid_t owner, gid_t group);
int kill(pid_t pid, int sig);
int access(const char *path, int mode);
int isatty(int fd);
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif
