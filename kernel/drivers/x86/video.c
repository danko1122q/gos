#include "drivers/video.h"
#include "arch/io.h"
#include "mem/pmm.h"
#include "lib/string.h"

#define BOOT_FB_ADDR    0x600
#define BOOT_FB_PITCH   0x604

extern void blit32(void *dst, const void *src, u32 dwords);

#define VBUF_BYTES  (SCREEN_W * SCREEN_H)
#define VBUF_FRAMES ((VBUF_BYTES + 4095) / 4096)

static u8 *scene, *back;
static u8 *fb;
static u32 pitch;

void video_init(void) {
    u32 fb_addr;
    u16 fb_pitch;
    __asm__ volatile("movl (%1), %0" : "=r"(fb_addr) : "r"((u32)BOOT_FB_ADDR) : "memory");
    __asm__ volatile("movw (%1), %0" : "=r"(fb_pitch) : "r"((u32)BOOT_FB_PITCH) : "memory");
    fb = (u8 *)fb_addr;
    pitch = fb_pitch;
}

int video_buffers_init(void) {
    u32 p = pmm_alloc_contig(2 * VBUF_FRAMES);
    if (!p) return -1;
    memset((void *)p, 0, 2 * VBUF_FRAMES * 4096u);
    back = (u8 *)p;
    scene = (u8 *)(p + VBUF_FRAMES * 4096u);
    return 0;
}

void video_set_palette(int i, int r, int g, int b) {
    outb(0x3C8, i); outb(0x3C9, r >> 2); outb(0x3C9, g >> 2); outb(0x3C9, b >> 2);
}

u8 *video_scene(void) { return scene; }
u8 *video_back(void)  { return back; }

void video_compose(void) { blit32(back, scene, SCREEN_W * SCREEN_H / 4); }

void video_flip(void) {
    for (int y = 0; y < SCREEN_H; y++) blit32(fb + y * pitch, back + y * SCREEN_W, SCREEN_W / 4);
}

u32 video_fb_phys(void) { return (u32)fb; }
u32 video_fb_size(void) { return pitch * SCREEN_H; }
