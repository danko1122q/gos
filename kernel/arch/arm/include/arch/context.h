#ifndef ARCH_CONTEXT_H
#define ARCH_CONTEXT_H
#include "lib/types.h"

extern void switch_context(u32 *old_sp, u32 new_sp);
extern void enter_user(u32 pc, u32 usp, u32 ktop);

#define ARCH_ENTER_USER(t) enter_user((t)->user_eip, (t)->user_esp, (u32)(t)->stack + STACK_SZ)

static inline void arch_set_kstack(u32 top) { (void)top; }

static inline u32 arch_task_frame(u8 *stack, u32 size, void (*fn)(void)) {
    u32 *sp = (u32 *)(stack + size);
    *--sp = (u32)fn;
    for (int i = 0; i < 8; i++) *--sp = 0;
    return (u32)sp;
}

#endif
