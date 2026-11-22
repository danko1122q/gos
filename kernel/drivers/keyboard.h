#ifndef DRV_KEYBOARD_H
#define DRV_KEYBOARD_H
#include "abi.h"

void ps2_init(void);
void kbd_byte(unsigned char sc);
int  kbd_mods(void);

void kbd_tick(void);

#endif
