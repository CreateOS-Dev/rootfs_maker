// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space pwd command
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    char buf[256];
    if (sys_getcwd(buf, sizeof(buf)) >= 0) {
        sys_puts(buf, 0x0F);
        sys_putchar('\n');
        return 0;
    }
    return 1;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("pwd: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}