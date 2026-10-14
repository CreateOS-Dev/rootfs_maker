// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "user_syscall.h"

static const char* role_name(int role)
{
    switch (role) {
    case ROLE_GUEST: return "guest";
    case ROLE_USER: return "user";
    case ROLE_ADMIN: return "admin";
    case ROLE_ROOT: return "root";
    default: return "invalid";
    }
}

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    int count = sys_user_count();
    if (count < 0) {
        sys_puts("lsusers: unable to read account list\n", 0x0C);
        return 1;
    }

    sys_puts("Users:\n", 0x0E);
    for (int i = 0; i < count; i++) {
        user_t user;
        if (sys_user_get(i, &user) <= 0) {
            sys_puts("lsusers: account query failed\n", 0x0C);
            return 1;
        }
        printf("  %s role=%s%s\n", user.name, role_name(user.role),
               user.locked ? " locked" : "");
    }
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("lsusers: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}