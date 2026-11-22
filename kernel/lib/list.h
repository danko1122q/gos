#ifndef LIB_LIST_H
#define LIB_LIST_H
#include "lib/types.h"

typedef struct ListNode { struct ListNode *prev, *next; } ListNode;

static inline void list_init(ListNode *h) { h->prev = h->next = h; }
static inline int  list_empty(const ListNode *h) { return h->next == h; }
static inline void list_add_tail(ListNode *h, ListNode *n) {
    n->prev = h->prev; n->next = h;
    h->prev->next = n; h->prev = n;
}
static inline void list_del(ListNode *n) {
    n->prev->next = n->next; n->next->prev = n->prev;
    n->prev = n->next = n;
}
#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#endif
