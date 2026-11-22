#ifndef FS_PERSIST_H
#define FS_PERSIST_H
#include "lib/types.h"

#define FS_LBA      2048
#define FS_SECTORS  2048
#define FS_SLOTS    2

int  fs_load(void);
int  fs_sync(void);
void fs_make_defaults(void);
void fs_refresh_system(void);
u32  fs_last_sync_bytes(void);
int  fs_dirty(void);
int  fs_sync_pid(void);
struct Task;
void fs_sync_abort(struct Task *t);
void syncd_start(void);

#endif
