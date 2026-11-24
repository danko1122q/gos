#include "lib/printf.h"
#include "lib/klog.h"
#include "lib/string.h"
#include "mem/heap.h"
#include "arch/io.h"
#include "arch/idt.h"
#include "arch/arch.h"
#include "arch/timer.h"
#include "arch/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "proc/task.h"
#include "proc/exec.h"
#include "sys/syscall.h"
#include "drivers/video.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/rtc.h"
#include "drivers/ata.h"
#include "fs/vfs.h"
#include "fs/persist.h"
#include "gfx/draw.h"
#include "gfx/cursor.h"
#include "gui/event.h"
#include "gui/window.h"
#include "gui/desktop.h"
#include "gfx/font.h"
#include "sys/settings.h"

extern void initrd_mount(void);

#define MIN_HEAP_FRAMES  64u
#define MIN_PROC_FRAMES  (VMM_PD_FRAMES + 3u + 40u + USER_STACK_PAGES)
typedef struct { u32 video, pt, heap, proc, total; } MemNeed;
static MemNeed mem_need(u32 ram) {
    MemNeed n;
    u32 hsz = ((ram / 2) + 0x3FFFFFu) & ~0x3FFFFFu;
    if (hsz < 0x400000u) hsz = 0x400000u;
    if (hsz > 0x4000000u) hsz = 0x4000000u;
    n.video = VIDEO_NBUF * ((SCREEN_W * SCREEN_H + 4095) / 4096);
    n.pt    = vmm_boot_frames(ram, hsz);
    n.heap  = MIN_HEAP_FRAMES;
    n.proc  = MIN_PROC_FRAMES;
    n.total = n.video + n.pt + n.heap + n.proc;
    return n;
}

static void autostart(void) {
    char *d; u32 l;
    if (vfs_read_file(NULL, "/etc/autostart", &d, &l)) return;
    for (char *p = d; *p; ) {
        char *e = strchr(p, '\n');
        if (e) *e = 0;
        while (*p == ' ' || *p == '\t') p++;
        if (*p && *p != '#') launch_prog(p, NULL);
        if (!e) break;
        p = e + 1;
    }
    kfree(d);
}


#ifdef GOS_MEMBENCH
static void membench_tick(u32 sec) {
    const char *lbl = NULL;
    switch (sec) {
    case 2:  lbl = "base";    break;
    case 3:  launch_prog("files", NULL); break;
    case 4:  lbl = "files";    break;
    case 5:  launch_prog("terminal", NULL); break;
    case 6:  lbl = "+terminal"; break;
    case 7:  launch_prog("paint", NULL); break;
    case 9:  lbl = "+paint";   break;
    }
#ifdef GOS_STACKMARK
    if (sec == 9) klog("stack-hw: deepest kernel stack use %u of %u bytes\n", task_stack_hw(), (u32)STACK_SZ);
#endif
    if (lbl) klog("membench: %s pmm_free=%u heap=%u/%u KB\n", lbl, pmm_free_frames(), heap_used() / 1024, heap_total() / 1024);
}
#endif

void kmain(void) {
    serial_init();
    klog("GOS %s: boot (kernel in ring 0, applications in ring 3)\n", GOS_VERSION);
    arch_early_init();
    video_init();
    event_init();

    u32 ram = mem_total_kb() * 1024u;
    if (ram > PMM_MAX_RAM) ram = PMM_MAX_RAM;
    u32 ram_base = mem_base_kb() * 1024u;
    pmm_init(ram_base, ram);
    extern char __bss_end[];
    u32 f0 = pmm_free_frames();
    pmm_add_range((u32)__bss_end, 0x90000);
    u32 gap = pmm_free_frames() - f0;
    MemNeed need = mem_need(ram);
    if (pmm_free_frames() < need.total)
        kpanic("RAM %u KB is too small: need >= %u KB (1024 low/kernel + video %u + page table %u + heap %u + 1 process %u KB)",
               ram / 1024, 1024 + (need.total - gap) * 4, need.video * 4, need.pt * 4, need.heap * 4, need.proc * 4);
    if (video_buffers_init()) kpanic("no frames for video buffers");
    vmm_init(ram, video_fb_phys(), video_fb_size());
    heap_init();
    klog("mem: %u KB RAM (map %s, base %u KB, +%u KB bss gap), dynamic heap %u KB (peak %u KB, region %p +%u MB), %u free frames\n",
         mem_total_kb(), mem_map_source(), mem_base_kb(), gap * 4, heap_total() / 1024, heap_peak() / 1024, vmm_heap_base(), vmm_heap_size() >> 20, pmm_free_frames());

    task_init();
    syscall_init();
    timer_init();
    rtc_init();
    ata_init();

    vfs_init();
    for (int i = 0; i < 3; i++) fd_open("/dev/console", O_RDWR);
    if (!fs_load()) { fs_make_defaults(); klog("fs: creating initial tree\n"); }
    else fs_refresh_system();
    initrd_mount();

    settings_init();
    mouse_set_bounds(SCREEN_W, SCREEN_H);
    ps2_init();
    wm_init();
    font_init();
    desktop_init(mouse_x(), mouse_y());
    rtc_poll();
    syncd_start();

    sti();
    klog("GOS: init ok, entering event loop (pmm free %u frames, heap %u/%u KB)\n", pmm_free_frames(), heap_used() / 1024, heap_total() / 1024);
    autostart();

    u32 last_sec = timer_ticks();
    for (;;) {
        preempt_disable();
        task_reap();
        Event e;
        while (event_pop(&e)) desktop_handle_event(&e);

        if (timer_ticks() - last_sec >= HZ) {
            last_sec += HZ;
            rtc_poll();
            desktop_tick();
#ifdef GOS_MEMBENCH
            { static u32 bsec; membench_tick(++bsec); }
#endif
        }

        int d = desktop_take_dirty();
        if (d) {
            cursor_hide();
            if (d & DIRTY_SCENE) desktop_render();
            video_compose();
            draw_set_target(video_back());
            cursor_draw(mouse_x(), mouse_y());
            video_flip();
        }
        preempt_enable();

        cli();
        if (!event_pending() && !desktop_take_dirty_peek()) {
            sys_idle = 1;
            cpu_wait_irq();
            sys_idle = 0;
        } else sti();
    }
}
