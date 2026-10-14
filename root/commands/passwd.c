// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space passwd command - changes password via sys_user_setpass
 */
#include "user_syscall.h"

static int read_password(char* buf, int max, const char* prompt) {
    sys_puts(prompt, 0x0F);
    int len = 0;
    while (len < max - 1) {
        char c = sys_getc();
        if (c == '\n' || c == '\r') {
            sys_putchar('\n');
            break;
        }
        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                sys_putchar('\b');
                sys_putchar(' ');
                sys_putchar('\b');
            }
            continue;
        }
        if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F) continue;
        buf[len++] = c;
        sys_putchar('*');
    }
    buf[len] = 0;
    return len;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    user_t user;
    if (sys_user_current(&user) <= 0) {
        sys_puts("passwd: no user logged in\n", 0x0C);
        return 1;
    }

    char oldpass[32];
    char newpass1[32];
    char newpass2[32];

    if (read_password(oldpass, sizeof(oldpass), "Current password: ") == 0) {
        sys_puts("\nEmpty password not allowed\n", 0x0C);
        memset(oldpass, 0, sizeof(oldpass));
        return 1;
    }

    if (read_password(newpass1, sizeof(newpass1), "New password: ") == 0) {
        sys_puts("\nEmpty password not allowed\n", 0x0C);
        memset(oldpass, 0, sizeof(oldpass));
        memset(newpass1, 0, sizeof(newpass1));
        return 1;
    }

    if (read_password(newpass2, sizeof(newpass2), "Retype new password: ") == 0) {
        sys_puts("\nEmpty password not allowed\n", 0x0C);
        memset(oldpass, 0, sizeof(oldpass));
        memset(newpass1, 0, sizeof(newpass1));
        memset(newpass2, 0, sizeof(newpass2));
        return 1;
    }

    if (strcmp(newpass1, newpass2) != 0) {
        sys_puts("\nPasswords do not match\n", 0x0C);
        memset(oldpass, 0, sizeof(oldpass));
        memset(newpass1, 0, sizeof(newpass1));
        memset(newpass2, 0, sizeof(newpass2));
        return 1;
    }

    if (sys_user_verify_password(oldpass) <= 0) {
        sys_puts("\nIncorrect password\n", 0x0C);
        memset(oldpass, 0, sizeof(oldpass));
        memset(newpass1, 0, sizeof(newpass1));
        memset(newpass2, 0, sizeof(newpass2));
        return 1;
    }

    int result = sys_user_setpass(user.name, newpass1, oldpass);
    memset(oldpass, 0, sizeof(oldpass));
    memset(newpass1, 0, sizeof(newpass1));
    memset(newpass2, 0, sizeof(newpass2));
    if (result <= 0) {
        sys_puts("\nFailed to change password\n", 0x0C);
        return 1;
    }

    sys_puts("\nPassword changed successfully\n", 0x0A);
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("passwd: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}