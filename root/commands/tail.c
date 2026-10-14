// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space tail command
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sys_puts("tail: not fully implemented\n", 0x0C);
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("tail: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}