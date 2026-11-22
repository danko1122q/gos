#include "gos.h"

int main(void) {
    char b[256];
    int n;
    while ((n = gos_read(0, b, sizeof b)) > 0) {
        for (int i = 0; i < n; i++) if (b[i] >= 'a' && b[i] <= 'z') b[i] -= 32;
        if (gos_write(1, b, (u32)n) < 0) return 1;
    }
    return n < 0 ? 1 : 0;
}
