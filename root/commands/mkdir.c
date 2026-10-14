// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space mkdir command
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (argc < 2) {
        sys_puts("Usage: mkdir <dir>...\n", 0x0C);
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (sys_mkdir(argv[i], 0755) < 0) {
            printf("mkdir: cannot create directory '%s'\n", argv[i]);
            ret = 1;
        }
    }
    return ret;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("mkdir: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}