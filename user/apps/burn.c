#include "gos.h"

int main(void) {
    volatile u32 x = 0;
    for (;;) x++;
}
