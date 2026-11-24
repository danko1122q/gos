#include "arch/panic.h"
#include "arch/io.h"
#include "lib/printf.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "drivers/video.h"

static const char *names[] = { "?", "Undefined instruction", "Software interrupt", "Prefetch abort", "Data abort", "IRQ" };

static void show(const char *l[], int n) {
    if (!video_back()) return;   /* panic before video_buffers_init(): back is NULL, drawing would overwrite low RAM (the kernel itself) */
    draw_set_target(video_back()); draw_noclip();
    draw_rect(0, 0, 640, 480, C_BLU);
    text_draw(24, 24, "*** GOS KERNEL PANIC ***", C_WHT, -1);
    for (int i = 0; i < n; i++) text_draw(24, 56 + i * 18, l[i], C_WHT, -1);
    text_draw(24, 56 + (n + 1) * 18, "System halted. Press reset or close QEMU.", C_YEL, -1);
    video_flip();
}

static void halt_forever(void) { cli(); for (;;) hlt(); }

void panic_exception(Regs *r) {
    char b[6][80]; const char *l[6];
    const char *nm = r->int_no < 14 ? "Exception" : (r->int_no == 14 ? "Page fault (abort)" : r->int_no == 17 ? "Alignment fault" : "Exception");
    ksnprintf(b[0], 80, "Exception %u: %s (err=%08x)", r->int_no, nm, r->err);
    ksnprintf(b[1], 80, "PC=%08x CPSR=%08x ADDR=%08x", r->pc, r->cpsr, r->addr);
    ksnprintf(b[2], 80, "R0=%08x R1=%08x R2=%08x R3=%08x", r->r[0], r->r[1], r->r[2], r->r[3]);
    ksnprintf(b[3], 80, "R4=%08x R5=%08x R6=%08x R7=%08x", r->r[4], r->r[5], r->r[6], r->r[7]);
    ksnprintf(b[4], 80, "SP=%08x LR=%08x R11=%08x R12=%08x", r->sp, r->lr, r->r[11], r->r[12]);
    for (int i = 0; i < 5; i++) { l[i] = b[i]; kprintf("PANIC %s\n", b[i]); }
    (void)names;
    show(l, 5);
    halt_forever();
    for (;;) ;
}

void kpanic(const char *fmt, ...) {
    char b[192]; const char *l[1] = { b };
    va_list ap; va_start(ap, fmt);
    kvsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    kprintf("PANIC %s\n", b);
    show(l, 1);
    halt_forever();
    for (;;) ;
}
