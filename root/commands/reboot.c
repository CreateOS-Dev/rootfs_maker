// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space reboot command - restart the system
 * Uses sys_exit with special code or a dedicated sys_reboot syscall
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    sys_puts("Rebooting...\n", 0x0E);

    /* Use sys_exit with special code for reboot signal */
    sys_exit(0xDEAD);

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    (void)main(0, (char**)0);
}