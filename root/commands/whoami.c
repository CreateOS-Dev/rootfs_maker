// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space whoami command
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    user_t user;
    if (sys_user_current(&user) > 0) {
        const char* role = "unknown";
        switch (user.role) {
        case ROLE_GUEST: role = "guest"; break;
        case ROLE_USER: role = "user"; break;
        case ROLE_ADMIN: role = "admin"; break;
        case ROLE_ROOT: role = "root"; break;
        }
        printf("%s (%s)\n", user.name, role);
    } else {
        sys_puts("unknown\n", 0x0C);
        return 1;
    }
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("whoami: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}