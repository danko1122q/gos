#include "fs/vfs.h"
#include "lib/printf.h"
#include "lib/klog.h"

typedef struct { const char *name; const u8 *data; u32 size; } InitrdEnt;
extern const InitrdEnt initrd_table[];
extern const u32 initrd_count;

void initrd_mount(void) {
    VNode *bin = vfs_lookup(NULL, "/bin");
    u32 total = 0;
    for (u32 i = 0; i < initrd_count; i++) {
        VNode *n = vfs_add_virt_blob(bin, initrd_table[i].name, initrd_table[i].data, initrd_table[i].size);
        if (n) { n->mode = 0755; total += initrd_table[i].size; }
    }
    klog("initrd: %u programs (%u KB) in /bin\n", initrd_count, total / 1024);
}
