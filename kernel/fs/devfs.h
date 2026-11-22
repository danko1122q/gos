#ifndef FS_DEVFS_H
#define FS_DEVFS_H
#include "fs/vfs.h"
void devfs_init(VNode *dev);
void procfs_init(VNode *proc);
#endif
