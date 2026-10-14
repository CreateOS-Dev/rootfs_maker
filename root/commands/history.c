// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space history command - show command history
 * Uses shell's internal history (no syscall needed)
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    sys_puts("history: command history is maintained by the shell\n", 0x0F);
    sys_puts("Use up/down arrow keys in shell to navigate history\n", 0x0F);

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("history: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}