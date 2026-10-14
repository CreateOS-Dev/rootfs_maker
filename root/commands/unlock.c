// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space unlock command - unlock user account
 * Uses sys_user_unlock syscall
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        sys_puts("Usage: unlock <username>\n", 0x0C);
        return 1;
    }

    int ret = sys_user_unlock(argv[1]);
    if (ret <= 0) {
        sys_puts("unlock: failed to unlock user\n", 0x0C);
        return 1;
    }

    sys_puts("User unlocked\n", 0x0A);
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("unlock: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}