// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space groupadd command - add a new group
 * Uses sys_group_add syscall (to be implemented)
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        sys_puts("Usage: groupadd <groupname>\n", 0x0C);
        return 1;
    }

    sys_puts("groupadd: group management not yet implemented in kernel\n", 0x0E);
    sys_puts("Use 'adduser' to create users with roles instead\n", 0x0E);

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("groupadd: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}