#include "drivers/keyboard.h"
#include "drivers/kbd_repeat.h"
#include "drivers/mouse.h"
#include "sys/settings.h"
#include "gui/event.h"

static const char lo[0x59] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0, '\\','z','x','c','v',
    'b','n','m',',','.','/', 0, '*', 0, ' '
};
static const char up[0x59] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b', '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0, '|','Z','X','C','V',
    'B','N','M','<','>','?', 0, '*', 0, ' '
};

static int mods, caps, ext;

static KbdRepeat repeat_key;
static volatile u32 kticks;

int kbd_mods(void) { return mods; }

static void push(int key) {
    Event e = { .type = EV_KEY_DOWN, .a = key, .b = mods };
    event_push(&e);
}

static void push_rep(int key, int sc) {
    push(key);
    if (mods & (MOD_CTRL | MOD_ALT)) { kbd_repeat_stop(&repeat_key); return; }
    kbd_repeat_start(&repeat_key, key, sc, kticks, (KbdU32)set_key_delay_ticks());
}

void kbd_tick(void) {
    kticks++;
    int key = kbd_repeat_tick(&repeat_key, kticks, (KbdU32)set_key_rate_ticks(), mods);
    if (key >= 0 && !event_full()) push(key);
}

void kbd_byte(unsigned char sc) {
    if (sc == 0xE0) { ext = 1; return; }
    int rel = sc & 0x80, c = sc & 0x7F, e = ext; ext = 0;
    int id = c + (e ? 0x100 : 0);
    if (c == 0x2A || c == 0x36) { mods = rel ? mods & ~MOD_SHIFT : mods | MOD_SHIFT; return; }
    if (c == 0x1D) { mods = rel ? mods & ~MOD_CTRL : mods | MOD_CTRL; return; }
    if (c == 0x38) { mods = rel ? mods & ~MOD_ALT : mods | MOD_ALT; return; }
    if (rel) { kbd_repeat_release(&repeat_key, id); return; }
    if (repeat_key.active && id == repeat_key.scancode) return;
    kbd_repeat_stop(&repeat_key);
    if (c == 0x3A && !e) { caps ^= 1; return; }
    if (e) {
        switch (c) {
        case 0x48: push_rep(KEY_UP, id); break;   case 0x50: push_rep(KEY_DOWN, id); break;
        case 0x4B: push_rep(KEY_LEFT, id); break;  case 0x4D: push_rep(KEY_RIGHT, id); break;
        case 0x47: push_rep(KEY_HOME, id); break;  case 0x4F: push_rep(KEY_END, id); break;
        case 0x49: push_rep(KEY_PGUP, id); break;  case 0x51: push_rep(KEY_PGDN, id); break;
        case 0x53: push_rep(KEY_DEL, id); break;   case 0x52: push(KEY_INS); break;
        case 0x1C: push_rep('\n', id); break;      case 0x35: push_rep('/', id); break;
        }
        return;
    }
    if (c >= 0x3B && c <= 0x44) { push(KEY_F1 + (c - 0x3B)); return; }
    if (c >= 0x59) return;
    int ch = (mods & MOD_SHIFT) ? up[c] : lo[c];
    if (!ch) return;
    if (caps && ch >= 'a' && ch <= 'z' && !(mods & MOD_SHIFT)) ch -= 32;
    else if (caps && ch >= 'A' && ch <= 'Z' && (mods & MOD_SHIFT)) ch += 32;
    if (mods & MOD_CTRL) { if (ch >= 'A' && ch <= 'Z') ch += 32; }
    push_rep(ch, id);
}
