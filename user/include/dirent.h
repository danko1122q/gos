#ifndef GOS_DIRENT_H
#define GOS_DIRENT_H
#include "abi.h"
#include <sys/types.h>
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define IFTODT(mode) (((mode) & 0170000) >> 12)
#define DTTOIF(type) ((type) << 12)
struct dirent {
    ino_t d_ino;
    off_t d_off;
    u16 d_reclen;
    u8 d_type;
    char d_name[GOS_NAME_MAX];
    u32 d_size;
};
typedef struct __gos_DIR DIR;
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
void rewinddir(DIR *dirp);
long telldir(DIR *dirp);
void seekdir(DIR *dirp, long location);
int dirfd(DIR *dirp);
#endif
