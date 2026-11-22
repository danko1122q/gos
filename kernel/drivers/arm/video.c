#include "drivers/video.h"
#include "arch/hw.h"
#include "mem/pmm.h"
#include "lib/string.h"
#include "lib/printf.h"

#define CLCD_TIM0  0x00
#define CLCD_TIM1  0x04
#define CLCD_TIM2  0x08
#define CLCD_TIM3  0x0C
#define CLCD_UBAS  0x10
#define CLCD_LBAS  0x14
#define CLCD_CNTL  0x1C
#define CLCD_CNTL2 0x18
#define CLCD_PAL   0x200

#define VBUF_BYTES  (SCREEN_W * SCREEN_H)
#define VBUF_FRAMES ((VBUF_BYTES + 4095) / 4096)

static u8 *scene, *back, *fb;
static int lcd_on;

static void clcd_cntl(u32 v) { wr32(CLCD_BASE + CLCD_CNTL, v); wr32(CLCD_BASE + CLCD_CNTL2, v); }

static void lcd_enable(void) {
    wr32(CLCD_BASE + CLCD_TIM0, 0x3F1F3F9Cu);
    wr32(CLCD_BASE + CLCD_TIM1, 0x090B61DFu);
    wr32(CLCD_BASE + CLCD_TIM2, 0x067F1800u);
    wr32(CLCD_BASE + CLCD_TIM3, 0);
    wr32(CLCD_BASE + CLCD_UBAS, (u32)fb);
    wr32(CLCD_BASE + CLCD_LBAS, 0);
    u32 cr = (3u << 1) | (1u << 5);
    clcd_cntl(cr);
    clcd_cntl(cr | 1u);
    clcd_cntl(cr | 1u | (1u << 11));
    wr32(CLCD_BASE + CLCD_TIM0, 0x3F1F3F98u);
    wr32(CLCD_BASE + CLCD_TIM1, 0x090B61DEu);
    wr32(CLCD_BASE + CLCD_TIM0, 0x3F1F3F9Cu);
    wr32(CLCD_BASE + CLCD_TIM1, 0x090B61DFu);
    lcd_on = 1;
    kprintf("video: CLCD enabled, fb=%p CNTL(0x18)=%08x CNTL(0x1C)=%08x TIM0=%08x TIM1=%08x\n", (u32)fb, rd32(CLCD_BASE + CLCD_CNTL2), rd32(CLCD_BASE + CLCD_CNTL), rd32(CLCD_BASE + CLCD_TIM0), rd32(CLCD_BASE + CLCD_TIM1));
}

void video_init(void) {}

int video_buffers_init(void) {
    u32 p = pmm_alloc_contig(VIDEO_NBUF * VBUF_FRAMES);
    if (!p) return -1;
    memset((void *)p, 0, VIDEO_NBUF * VBUF_FRAMES * 4096u);
    back = (u8 *)p;
    scene = back + VBUF_FRAMES * 4096u;
    fb = scene + VBUF_FRAMES * 4096u;
    return 0;
}

static u16 pal_shadow[256];

void video_set_palette(int i, int r, int g, int b) {
    i &= 255;
    pal_shadow[i] = (u16)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
    wr32(CLCD_BASE + CLCD_PAL + (u32)(i >> 1) * 4, (u32)pal_shadow[i & ~1] | ((u32)pal_shadow[i | 1] << 16));
    if (!lcd_on && fb) lcd_enable();
}

u8 *video_scene(void) { return scene; }
u8 *video_back(void)  { return back; }

static void copy32(void *dst, const void *src, u32 dwords) {
    u32 *d = dst; const u32 *s = src;
    while (dwords--) *d++ = *s++;
}

void video_compose(void) { copy32(back, scene, VBUF_BYTES / 4); }
void video_flip(void) {
    if (!lcd_on && fb) lcd_enable();
    copy32(fb, back, VBUF_BYTES / 4);
}

u32 video_fb_phys(void) { return 0; }
u32 video_fb_size(void) { return 0; }
