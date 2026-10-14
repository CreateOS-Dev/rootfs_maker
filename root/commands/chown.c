// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space chown command
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (argc < 3) {
        sys_puts("Usage: chown <user> <file>...\n", 0x0C);
        return 1;
    }

    int uid = atoi(argv[1]);
    int ret = 0;

    for (int i = 2; i < argc; i++) {
        if (sys_chown(argv[i], uid, -1) < 0) {
            printf("chown: cannot access '%s'\n", argv[i]);
            ret = 1;
        }
    }
    return ret;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("chown: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}