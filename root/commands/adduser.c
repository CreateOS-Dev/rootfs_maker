// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space adduser command - adds user via syscalls
 */
#include "user_syscall.h"
#include "../../../kernel/header/proc/user_name.h"

static int read_input(char* buf, size_t capacity, int hide_input)
{
    int len = 0;
    int overflow = 0;
    if (!buf || capacity < 2) return -1;
    for (;;) {
        char c = sys_getc();
        if (c == '\n' || c == '\r') {
            sys_putchar('\n');
            break;
        }
        if (c == '\b' || c == 127) {
            if (!overflow && len > 0) {
                len--;
                sys_putchar('\b');
                sys_putchar(' ');
                sys_putchar('\b');
            }
            continue;
        }
        if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F) continue;
        if ((size_t)len + 1 >= capacity) {
            overflow = 1;
            continue;
        }
        buf[len++] = c;
        sys_putchar(hide_input ? '*' : c);
    }
    buf[len] = 0;
    return overflow ? -1 : len;
}

int main(int argc, char* argv[]) {
    (void)argv;
    user_t current;
    char username[32];
    char pass1[64];
    char pass2[64];
    int length;

    if (argc != 1) {
        sys_puts("Usage: adduser\n", 0x0C);
        return 2;
    }
    if (sys_user_current(&current) <= 0 ||
        (current.role != ROLE_ADMIN && current.role != ROLE_ROOT)) {
        sys_puts("adduser: permission denied (administrator required)\n", 0x0C);
        return 1;
    }

    sys_puts("Username: ", 0x0F);
    length = read_input(username, sizeof(username), 0);
    if (length < 0) {
        sys_puts("Username is too long\n", 0x0C);
        return 1;
    }
    if (length == 0 || !user_name_creation_valid(username)) {
        sys_puts("Username must contain only letters (A-Z, a-z), with no digits or symbols\n",
                 0x0C);
        return 1;
    }
    if (strcmp(username, current.name) == 0) {
        sys_puts("adduser: you cannot create an account with your own username\n",
                 0x0C);
        return 1;
    }
    user_t existing;
    int found = sys_user_find(username, &existing);
    if (found < 0) {
        sys_puts("adduser: account lookup failed\n", 0x0C);
        return 1;
    }
    if (found > 0) {
        sys_puts("adduser: username already exists\n", 0x0C);
        return 1;
    }

    for (;;) {
        memset(pass1, 0, sizeof(pass1));
        memset(pass2, 0, sizeof(pass2));
        sys_puts("Password: ", 0x0F);
        length = read_input(pass1, sizeof(pass1), 1);
        if (length < 0) {
            sys_puts("Password must be at most 63 characters\n", 0x0C);
            continue;
        }
        if (length == 0) {
            sys_puts("Password cannot be empty\n", 0x0C);
            continue;
        }
        sys_puts("Confirm: ", 0x0F);
        length = read_input(pass2, sizeof(pass2), 1);
        if (length < 0) {
            sys_puts("Confirmation must be at most 63 characters\n", 0x0C);
            continue;
        }
        if (strcmp(pass1, pass2) == 0) break;
        sys_puts("Passwords do not match\n", 0x0C);
    }

    int ret = sys_user_add(username, pass1, ROLE_USER);
    memset(pass1, 0, sizeof(pass1));
    memset(pass2, 0, sizeof(pass2));
    if (ret <= 0) {
        if (ret == USER_ADD_NO_SECURE_RANDOM)
            sys_puts("adduser: secure random source unavailable; enable RDSEED/RDRAND\n",
                     0x0C);
        else if (ret == USER_ADD_LIMIT_REACHED)
            sys_puts("adduser: account limit reached\n", 0x0C);
        else if (ret == USER_ADD_DATABASE_FAILED)
            sys_puts("adduser: cannot persist account database\n", 0x0C);
        else if (ret == USER_ADD_PERMISSION_DENIED)
            sys_puts("adduser: permission denied\n", 0x0C);
        else
            sys_puts("adduser: invalid account data or account already exists\n",
                     0x0C);
        return 1;
    }

    user_t created_user;
    if (sys_user_find(username, &created_user) <= 0 ||
        !user_setup_home(&created_user)) {
        sys_puts("adduser: account was saved, but its home setup failed; "
                 "login will retry setup\n", 0x0C);
        return 1;
    }
    sys_puts("\nUser created successfully\n", 0x0A);
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("adduser: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}