#include "gos.h"

int main(void) {
    gfs_write("/tmp/counter.txt", "", 0, 0);
    for (int i = 1; i <= 10; i++) {
        char b[32];
        int l = ksnprintf(b, sizeof b, "count %d\n", i);
        gfs_write("/tmp/counter.txt", b, (u32)l, 1);
        gos_sleep_ms(800);
    }
    return 0;
}
