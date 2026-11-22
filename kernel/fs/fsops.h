#ifndef FS_FSOPS_H
#define FS_FSOPS_H
#include "lib/types.h"

int  trash_move(const char *path);
int  trash_putaway(const char *path);
int  trash_empty(void);
void trash_info(u32 *items, u32 *bytes);
int  fs_duplicate(const char *path);
int  fs_unique_name(const char *dir, const char *base, const char *suffix, char *out, int max);

#endif
