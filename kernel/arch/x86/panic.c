#include "arch/panic.h"
#include "arch/io.h"
#include "lib/printf.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "drivers/video.h"

static const char *names[20] = {
    "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range", "Invalid opcode",
    "Device not available", "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment not present",
    "Stack fault", "General protection", "Page fault", "Reserved", "x87 FPU error", "Alignment check",
    "Machine check", "SIMD error"
};

static void show(const char *l[], int n) {
    draw_set_target(video_back()); draw_noclip();
    draw_rect(0, 0, 640, 480, C_BLU);
    text_draw(24, 24, "*** GOS KERNEL PANIC ***", C_WHT, -1);
    for (int i = 0; i < n; i++) text_draw(24, 56 + i * 18, l[i], C_WHT, -1);
    text_draw(24, 56 + (n + 1) * 18, "System halted. Press reset or close QEMU.", C_YEL, -1);
    video_flip();
}

static void halt_forever(void) { cli(); for (;;) hlt(); }

void panic_exception(Regs *r) {
    u32 cr2; __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    char b[6][80]; const char *l[6];
    const char *nm = r->int_no < 20 ? names[r->int_no] : "Exception";
    ksnprintf(b[0], 80, "Exception %u: %s (err=%08x)", r->int_no, nm, r->err);
    ksnprintf(b[1], 80, "EIP=%08x CS=%04x EFLAGS=%08x CR2=%08x", r->eip, r->cs, r->eflags, cr2);
    ksnprintf(b[2], 80, "EAX=%08x EBX=%08x ECX=%08x EDX=%08x", r->eax, r->ebx, r->ecx, r->edx);
    ksnprintf(b[3], 80, "ESI=%08x EDI=%08x EBP=%08x", r->esi, r->edi, r->ebp);
    for (int i = 0; i < 4; i++) { l[i] = b[i]; kprintf("PANIC %s\n", b[i]); }
    show(l, 4);
    halt_forever();
    for (;;) ;
}

void kpanic(const char *fmt, ...) {
    char b[100]; const char *l[1] = { b };
    va_list ap; va_start(ap, fmt);
    kvsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    kprintf("PANIC %s\n", b);
    show(l, 1);
    halt_forever();
    for (;;) ;
}
