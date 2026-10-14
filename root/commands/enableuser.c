// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space enableuser command - enable user account
 * Uses sys_user_enable syscall
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        sys_puts("Usage: enableuser <username>\n", 0x0C);
        return 1;
    }

    int ret = sys_user_enable(argv[1]);
    if (ret <= 0) {
        sys_puts("enableuser: failed to enable user\n", 0x0C);
        return 1;
    }

    sys_puts("User enabled\n", 0x0A);
    return 0;
}

void _start(void) {
    char name[32];
    int len = 0;
    sys_puts("Username to enable: ", 0x0F);
    while (len < (int)sizeof(name) - 1) {
        char c = sys_getc();
        if (c == '\n' || c == '\r') break;
        if (c == 127 && len > 0) { len--; sys_puts("\b \b", 0x0F); continue; }
        if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7F) { name[len++] = c; sys_putchar(c); }
    }
    name[len] = 0;
    sys_putchar('\n');
    int result = sys_user_enable(name);
    if (result <= 0) sys_puts("enableuser: failed\n", 0x0C);
    else sys_puts("User enabled\n", 0x0A);
    sys_exit(result > 0 ? 0 : 1);
}