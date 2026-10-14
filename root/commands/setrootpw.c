// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space setrootpw command - set root password
 * Uses sys_user_setpass syscall (requires root)
 */
#include "user_syscall.h"

static int read_password(char* buf, int max) {
    int len = 0;
    sys_puts("New password: ", 0x0F);
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

    /* Check if current user is root */
    user_t current;
    if (sys_user_current(&current) <= 0 || strcmp(current.name, "root") != 0) {
        sys_puts("setrootpw: permission denied (requires root)\n", 0x0C);
        return 1;
    }

    char pass1[32], pass2[32];
    if (read_password(pass1, sizeof(pass1)) == 0) {
        sys_puts("Password cannot be empty\n", 0x0C);
        return 1;
    }

    sys_puts("Retype password: ", 0x0F);
    if (read_password(pass2, sizeof(pass2)) == 0 || strcmp(pass1, pass2) != 0) {
        sys_puts("\nSorry, passwords do not match\n", 0x0C);
        return 1;
    }

    int ret = sys_user_setpass("root", pass1, NULL);
    if (ret <= 0) {
        sys_puts("\nFailed to set root password\n", 0x0C);
        return 1;
    }

    sys_puts("\nRoot password updated successfully\n", 0x0A);
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("setrootpw: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}