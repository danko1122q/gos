#ifndef SYS_SETTINGS_H
#define SYS_SETTINGS_H
#include "abi.h"

extern volatile int g_set[SET_COUNT];

void settings_init(void);
int  settings_set(int id, int v);
static inline int settings_get(int id) { return (id >= 0 && id < SET_COUNT) ? g_set[id] : 0; }

int  set_dblclick_ms(void);
int  set_key_delay_ticks(void);
int  set_key_rate_ticks(void);
void set_mouse_scale(int *num, int *den);

#endif
