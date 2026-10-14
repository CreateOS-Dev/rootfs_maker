// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space touch command
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (argc < 2) {
        sys_puts("Usage: touch <file>...\n", 0x0C);
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        int fd = sys_open(argv[i], O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            printf("touch: cannot touch '%s'\n", argv[i]);
            ret = 1;
        } else {
            sys_close(fd);
        }
    }
    return ret;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("touch: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}