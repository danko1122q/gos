#include "gos.h"

extern int main(int argc, char **argv);

void __libc_start(u32 *sp) {
    int argc = (int)sp[0];
    char **argv = (char **)(sp + 1);
    gos_exit(main(argc, argv));
}
