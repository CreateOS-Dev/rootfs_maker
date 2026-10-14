// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space clear command
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sys_clear();
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_clear();
    sys_exit(0);
}