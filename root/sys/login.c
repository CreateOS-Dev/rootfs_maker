// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * userspace login program
 * Uses syscalls via userlib
 */
#include "user_syscall.h"

static void login_readline(char* buf, int max) {
    int len = 0;
    buf[0] = 0;

    while (1) {
        char c = sys_getc();
        unsigned char key = (unsigned char)c;
        if (!c) continue;

        if (c == '\n') {
            sys_putchar('\n');
            buf[len] = 0;
            return;
        }

        if (key >= KEY_LEFT && key <= KEY_TTY_NEXT) {
            /* Editing and console-switch keys are not credential characters. */
            continue;
        }

        if (c == '\b' || c == 127 || (unsigned char)c == KEY_DELETE) {
            if (len > 0) {
                len--;
                buf[len] = 0;
                sys_putchar('\b');
                sys_putchar(' ');
                sys_putchar('\b');
            }
            continue;
        }

        if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F)
            continue;

        if (len < max - 1) {
            buf[len++] = c;
            buf[len] = 0;
            sys_putchar(c);
        }
    }
}

static void login_read_password(char* buf, int max) {
    int len = 0;
    buf[0] = 0;

    while (1) {
        char c = sys_getc();
        unsigned char key = (unsigned char)c;
        if (!c) continue;

        if (c == '\n') {
            sys_putchar('\n');
            buf[len] = 0;
            return;
        }

        if (key >= KEY_LEFT && key <= KEY_TTY_NEXT) {
            continue;
        }

        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                buf[len] = 0;
                sys_putchar('\b');
                sys_putchar(' ');
                sys_putchar('\b');
            }
            continue;
        }

        if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F)
            continue;

        if (len < max - 1) {
            buf[len++] = c;
            buf[len] = 0;
            sys_putchar('*');
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    char user[32];
    char pass[32];
    int fail = 0;

    char tty[8];
    sys_tty_name(tty);
    sys_clear();
    (void)sys_tty_ctrl_c_interrupt(0);
    while (1) {
        sys_tty_name(tty);
        sys_puts("========================================\n", 0x0B);
        sys_puts(" CreateOS Login - ", 0x0E);
        sys_puts(tty, 0x0A);
        sys_puts("\n========================================\n", 0x0B);
        sys_puts("Username: ", 0x0F);
        login_readline(user, sizeof(user));

        if (!user[0]) {
            sys_puts("Username cannot be empty.\n", 0x0C);
            continue;
        }

        sys_puts("Password: ", 0x0F);
        login_read_password(pass, sizeof(pass));

        int ret = sys_user_login(user, pass);

        if (ret == USER_LOGIN_PROFILE_ERROR) {
            sys_puts("Account could not be prepared. Check its home filesystem "
                     "and the kernel serial log.\n", 0x0C);
            fail = 0;
            continue;
        }
        if (ret <= 0) {
            sys_puts("Login incorrect.\n", 0x0C);
            fail++;
            if (fail >= 3) {
                sys_puts("Too many attempts. Waiting...\n", 0x0C);
                sys_sleep(3000);
                fail = 0;
            }
            continue;
        }

        user_t account;
        if (sys_user_current(&account) <= 0 ||
            !user_setup_home(&account) || sys_chdir(account.home) < 0) {
            sys_puts("Login succeeded, but the home directory could not be prepared.\n",
                     0x0C);
            sys_user_logout();
            continue;
        }
        if (user_apply_keyboard_layout(&account) < 0)
            sys_puts("Warning: the saved keyboard layout could not be applied.\n",
                     0x0E);
        break;
    }

    sys_clear();
    (void)sys_tty_ctrl_c_interrupt(1);
    /* Launch the shell on a clean terminal. */
    char* argv_shell[] = { "/bin/sh", 0 };
    int exec_ret = sys_exec("/bin/sh", argv_shell);
    if (exec_ret < 0) {
        sys_puts("[login] Failed to exec shell\n", 0x0C);
    }

    return 0;
}

void _start(void) {
    (void)main(0, (char**)0);
    sys_exit(0);
}