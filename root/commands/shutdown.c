// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space shutdown command - halt the system
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    sys_puts("Shutting down...\n", 0x0E);

    /* Use sys_exit with special code for shutdown */
    sys_exit(0xBEEF);

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    (void)main(0, (char**)0);
}