// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space exit command - exit the shell
 * Uses syscalls via user_syscall.h
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sys_exit(0);
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("exit: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}