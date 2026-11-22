#ifndef ARCH_CONTEXT_H
#define ARCH_CONTEXT_H
#include "lib/types.h"
#include "arch/gdt.h"

extern void switch_context(u32 *old_sp, u32 new_sp);
extern void enter_user(u32 eip, u32 esp);

#define ARCH_ENTER_USER(t) enter_user((t)->user_eip, (t)->user_esp)

static inline void arch_set_kstack(u32 top) { tss_set_esp0(top); }

static inline u32 arch_task_frame(u8 *stack, u32 size, void (*fn)(void)) {
    u32 *sp = (u32 *)(stack + size);
    *--sp = (u32)fn;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    return (u32)sp;
}

#endif
