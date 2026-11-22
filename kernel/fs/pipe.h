#ifndef FS_PIPE_H
#define FS_PIPE_H
#include "lib/types.h"

typedef struct Pipe Pipe;

Pipe *pipe_new(void);
int   pipe_read(Pipe *p, void *buf, u32 n, int nonblock);
int   pipe_write(Pipe *p, const void *buf, u32 n, int nonblock);
void  pipe_close_end(Pipe *p, int end);

#endif
