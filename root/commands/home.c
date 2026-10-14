// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space home command - cd to user's home directory
 * Uses sys_user_current and sys_chdir syscalls
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    user_t current;
    if (sys_user_current(&current) <= 0) {
        sys_puts("home: not logged in\n", 0x0C);
        return 1;
    }

    if (current.home[0] == 0) {
        sys_puts("home: no home directory set\n", 0x0C);
        return 1;
    }

    if (sys_chdir(current.home) < 0) {
        sys_puts("home: failed to change directory\n", 0x0C);
        return 1;
    }

    sys_puts("Changed to ", 0x0A);
    sys_puts(current.home, 0x0A);
    sys_putchar('\n');

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("home: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}