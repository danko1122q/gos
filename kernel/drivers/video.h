#ifndef DRV_VIDEO_H
#define DRV_VIDEO_H
#include "lib/types.h"
#include "arch/arch.h"

#define SCREEN_W 640
#define SCREEN_H 480
#define VIDEO_BUF_KB (VIDEO_NBUF * ((SCREEN_W * SCREEN_H + 4095) / 4096) * 4)

void video_init(void);
int  video_buffers_init(void);
void video_set_palette(int i, int r, int g, int b);
u8  *video_scene(void);
u8  *video_back(void);
void video_compose(void);
void video_flip(void);
u32  video_fb_phys(void);
u32  video_fb_size(void);

#endif
