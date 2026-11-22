#include "mem/uaccess.h"
#include "mem/vmm.h"
#include "lib/string.h"
#include "lib/errno.h"

static int page_ok(u32 va, int write) {
    u32 e = vmm_walk(vmm_current(), va);
    if (!(e & PTE_P) && vmm_demand_fault(va)) e = vmm_walk(vmm_current(), va);
    if ((e & (PTE_P | PTE_U)) != (PTE_P | PTE_U)) return 0;
    if (write && !(e & PTE_W)) return 0;
    return 1;
}

int uaccess_ok(const void *p, u32 len, int write) {
    u32 a = (u32)p;
    if (!len) return 1;
    if (a < USER_BASE || a >= USER_TOP || len > USER_TOP - a) return 0;
    u32 end = a + len;
    for (u32 pg = a & PAGE_MASK; pg < end; pg += PAGE_SIZE) if (!page_ok(pg, write)) return 0;
    return 1;
}

int copy_from_user(void *dst, const void *usrc, u32 n) {
    if (!uaccess_ok(usrc, n, 0)) return -EFAULT;
    memcpy(dst, usrc, n);
    return 0;
}
int copy_to_user(void *udst, const void *src, u32 n) {
    if (!uaccess_ok(udst, n, 1)) return -EFAULT;
    memcpy(udst, src, n);
    return 0;
}

int strncpy_from_user(char *dst, const char *usrc, u32 max) {
    u32 a = (u32)usrc;
    if (!max) return -ENAMETOOLONG;
    for (u32 i = 0; i < max; i++) {
        u32 va = a + i;
        if (va < USER_BASE || va >= USER_TOP) return -EFAULT;
        if (i == 0 || !(va & (PAGE_SIZE - 1))) if (!page_ok(va & PAGE_MASK, 0)) return -EFAULT;
        char c = ((const char *)usrc)[i];
        dst[i] = c;
        if (!c) return (int)i;
    }
    dst[max - 1] = 0;
    return -ENAMETOOLONG;
}
