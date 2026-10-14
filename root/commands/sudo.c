// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * This build has no per-process credentials, so sudo cannot safely change
 * identity for a single command.
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        sys_puts("Usage: sudo [-u user] <command> [args...]\n", 0x0C);
        sys_puts("       sudo -i                    # root shell\n", 0x0C);
        return 1;
    }

    const char* target_user = "root";
    int arg_start = 1;
    char command_path[128];

    if (strcmp(argv[1], "-u") == 0) {
        if (argc < 4) {
            sys_puts("sudo: option -u requires an argument\n", 0x0C);
            return 1;
        }
        target_user = argv[2];
        arg_start = 3;
    } else if (strcmp(argv[1], "-i") == 0) {
        target_user = "root";
        arg_start = 2;
    }

    /* Find target user */
    user_t target;
    int target_result = sys_user_find(target_user, &target);
    if (target_result <= 0) {
        sys_puts("sudo: unknown user: ", 0x0C);
        sys_puts(target_user, 0x0C);
        sys_putchar('\n');
        return 1;
    }

    /* Get current user */
    user_t current;
    if (sys_user_current(&current) <= 0) {
        sys_puts("sudo: no current user\n", 0x0C);
        return 1;
    }

    if (strcmp(current.name, target_user) != 0) {
        sys_puts("sudo: secure user switching is unavailable until credentials are process-isolated\n",
                 0x0C);
        return 1;
    }

    if (current.role >= 2) {
        if (arg_start >= argc) {
            char* shell_argv[] = { "sh", NULL };
            sys_exec("/bin/sh", shell_argv);
            sys_puts("sudo: failed to execute shell\n", 0x0C);
            return 1;
        } else {
            if (argv[arg_start][0] == '/') {
                sys_exec(argv[arg_start], &argv[arg_start]);
            } else {
                snprintf(command_path, sizeof(command_path), "/bin/%s", argv[arg_start]);
                sys_exec(command_path, &argv[arg_start]);
            }
            sys_puts("sudo: failed to execute ", 0x0C);
            sys_puts(argv[arg_start], 0x0C);
            sys_putchar('\n');
            return 1;
        }
    }

    sys_puts("sudo: this account is not an administrator\n", 0x0C);
    return 1;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    (void)main(0, (char**)0);
}