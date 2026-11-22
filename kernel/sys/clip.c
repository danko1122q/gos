#include "sys/clip.h"
#include "mem/heap.h"
#include "lib/string.h"
#include "lib/errno.h"
#include "abi.h"

static u8 *data;
static u32 len;

int clip_set(const void *src, u32 n) {
    if (n > GOS_CLIP_MAX) return -EFBIG;
    u8 *nb = NULL;
    if (n) {
        nb = kmalloc(n);
        if (!nb) return -ENOMEM;
        memcpy(nb, src, n);
    }
    if (data) kfree(data);
    data = nb; len = n;
    return 0;
}

u32 clip_len(void) { return len; }

u32 clip_get(void *dst, u32 max) {
    u32 n = len < max ? len : max;
    if (n) memcpy(dst, data, n);
    return n;
}
