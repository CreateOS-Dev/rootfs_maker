// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "user_syscall.h"

static int parse_role(const char* value)
{
    if (strcmp(value, "guest") == 0 || strcmp(value, "0") == 0)
        return ROLE_GUEST;
    if (strcmp(value, "user") == 0 || strcmp(value, "1") == 0)
        return ROLE_USER;
    if (strcmp(value, "admin") == 0 || strcmp(value, "2") == 0)
        return ROLE_ADMIN;
    if (strcmp(value, "root") == 0 || strcmp(value, "3") == 0)
        return ROLE_ROOT;
    return -1;
}

int main(int argc, char* argv[])
{
    if (argc < 3 || argc > 4) {
        sys_puts("Usage: usermod <username> <option> [value]\n", 0x0C);
        sys_puts("Options: --password <newpass>, --role <guest|user|admin>, --lock, --unlock\n",
                 0x0C);
        return 1;
    }

    const char* username = argv[1];
    const char* option = argv[2];

    if (strcmp(option, "--password") == 0) {
        if (argc != 4) {
            sys_puts("Usage: usermod <username> --password <newpass>\n", 0x0C);
            return 1;
        }
        int ret = sys_user_setpass(username, argv[3], NULL);
        if (ret <= 0) {
            sys_puts("usermod: failed to change password\n", 0x0C);
            return 1;
        }
        sys_puts("Password changed\n", 0x0A);
    } else if (strcmp(option, "--lock") == 0) {
        if (argc != 3) return 1;
        int ret = sys_user_lock(username);
        if (ret <= 0) {
            sys_puts("usermod: failed to lock user\n", 0x0C);
            return 1;
        }
        sys_puts("User locked\n", 0x0A);
    } else if (strcmp(option, "--unlock") == 0) {
        if (argc != 3) return 1;
        int ret = sys_user_unlock(username);
        if (ret <= 0) {
            sys_puts("usermod: failed to unlock user\n", 0x0C);
            return 1;
        }
        sys_puts("User unlocked\n", 0x0A);
    } else if (strcmp(option, "--role") == 0) {
        int role;
        if (argc != 4 || (role = parse_role(argv[3])) < 0 ||
            role == ROLE_ROOT) {
            sys_puts("Usage: usermod <username> --role <guest|user|admin>\n",
                     0x0C);
            return 1;
        }
        int ret = sys_user_setrole(username, role);
        if (ret <= 0) {
            sys_puts("usermod: role change failed (admin required; root and current account are protected)\n",
                     0x0C);
            return 1;
        }
        sys_puts("User role changed\n", 0x0A);
    } else {
        sys_puts("usermod: unknown option\n", 0x0C);
        return 1;
    }

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("usermod: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}