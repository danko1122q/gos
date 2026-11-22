#ifndef DRIVERS_KBD_REPEAT_H
#define DRIVERS_KBD_REPEAT_H

typedef unsigned int KbdU32;
typedef signed int KbdS32;

enum { KBD_MOD_CTRL = 2, KBD_MOD_ALT = 4 };

typedef struct {
    int active;
    int key;
    int scancode;
    KbdU32 due;
} KbdRepeat;

static inline void kbd_repeat_stop(KbdRepeat *r) {
    r->active = 0;
}

static inline void kbd_repeat_start(KbdRepeat *r, int key, int scancode,
                                    KbdU32 now, KbdU32 delay) {
    r->active = 1;
    r->key = key;
    r->scancode = scancode;
    r->due = now + delay;
}

static inline void kbd_repeat_release(KbdRepeat *r, int scancode) {
    if (r->active && r->scancode == scancode) r->active = 0;
}

static inline int kbd_repeat_tick(KbdRepeat *r, KbdU32 now, KbdU32 rate, int mods) {
    if (!r->active || (KbdS32)(now - r->due) < 0) return -1;
    if (mods & (KBD_MOD_CTRL | KBD_MOD_ALT)) {
        r->active = 0;
        return -1;
    }
    r->due = now + rate;
    return r->key;
}

#endif
