// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space rm command - removes files via sys_unlink
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (argc < 2) {
        sys_puts("Usage: rm <file>...\n", 0x0C);
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (sys_unlink(argv[i]) < 0) {
            printf("rm: cannot remove '%s'\n", argv[i]);
            ret = 1;
        }
    }
    return ret;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("rm: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}