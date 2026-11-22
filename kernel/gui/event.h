#ifndef GUI_EVENT_H
#define GUI_EVENT_H
#include "lib/types.h"

typedef enum {
    EV_NONE = 0,
    EV_MOUSE_MOVE,
    EV_MOUSE_DOWN,
    EV_MOUSE_UP,
    EV_KEY_DOWN,
    EV_KEY_UP,
    EV_TIMER,
    EV_CLOCK,
    EV_MOUSE_WHEEL
} EventType;

typedef struct {
    EventType type;
    int x, y;
    int buttons;
    int a, b;
} Event;

void event_init(void);
int  event_push(const Event *e);
int  event_pop(Event *e);
int  event_full(void);
int  event_pending(void);

#endif
