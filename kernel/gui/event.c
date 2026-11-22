#include "gui/event.h"

#define QSIZE 256

static Event q[QSIZE];
static volatile u32 head, tail;

void event_init(void) { head = tail = 0; }
int  event_full(void) { return head - tail >= QSIZE - 8; }

int event_push(const Event *e) {
    if (head - tail >= QSIZE) return 0;
    q[head++ & (QSIZE - 1)] = *e;
    return 1;
}
int event_pop(Event *e) {
    if (head == tail) return 0;
    *e = q[tail++ & (QSIZE - 1)];
    return 1;
}
int  event_pending(void) { return head != tail; }
