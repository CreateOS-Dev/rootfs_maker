// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space logout command - logout current user
 * Uses sys_user_logout syscall
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    int ret = sys_user_logout();
    if (ret < 0) {
        sys_puts("logout: failed\n", 0x0C);
        return 1;
    }

    sys_puts("Logged out. Returning to login...\n", 0x0A);
    char* login_argv[] = { "/bin/login", NULL };
    if (sys_exec("/bin/login", login_argv) < 0) {
        sys_puts("logout: unable to start login\n", 0x0C);
        return 1;
    }
    return 0;
}