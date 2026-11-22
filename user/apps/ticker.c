#include "gos.h"

int main(void) {
    for (int n = 1;; n++) {
        char b[64];
        int l = ksnprintf(b, sizeof b, "tick %d uptime=%us\n", n, gos_ticks() / 100);
        GStat st;
        if (gos_stat("/tmp/ticker.txt", &st) == 0 && st.size > 1024) gfs_write("/tmp/ticker.txt", "", 0, 0);
        gfs_write("/tmp/ticker.txt", b, (u32)l, 1);
        gos_sleep_ms(1000);
    }
}
