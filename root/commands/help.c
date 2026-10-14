// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space help command - lists available commands from /bin/ and /usr/bin/
 * Also shows shell builtins (cd, exit, help)
 */
#include "user_syscall.h"

static void print_dir_commands(const char* path) {
    char names[128][USER_NAME_MAX];
    int n = sys_list(path, names, 128);

    if (n > 0) {
        sys_puts("\n----------------------------------------\n", 0x08);
        sys_puts(path, 0x0E);
        sys_puts("\n----------------------------------------\n", 0x0E);
        for (int i = 0; i < n; i++) {
            /* Skip hidden files and directories (those starting with .) */
            if (names[i][0] == '.') continue;
            size_t name_length = strlen(names[i]);
            if (name_length > 4 &&
                strcmp(names[i] + name_length - 4, ".bin") == 0)
                names[i][name_length - 4] = 0;
            sys_puts("  | ", 0x0F);
            sys_puts(names[i], 0x0F);
            sys_putchar('\n');
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    sys_puts("Available commands:\n", 0x0F);

    /* List /bin commands */
    print_dir_commands("/bin");

    /* List /usr/bin commands */
    print_dir_commands("/usr/bin");

    /* List /sbin commands */
    print_dir_commands("/sbin");

    /* List /usr/sbin commands */
    print_dir_commands("/usr/sbin");

    sys_puts("\n----------------------------------------\n", 0x08);
    sys_puts("Shell-control commands:\n", 0x0E);
    sys_puts("  cd <dir>       - change directory\n", 0x0F);
    sys_puts("  exit           - exit shell\n", 0x0F);
    sys_puts("  help           - show this help\n", 0x0F);
    sys_puts("  tty [tty1..tty5] - list or switch consoles\n", 0x0F);
    sys_puts("\nUse 'help' and command usage messages for available guidance.\n",
             0x0E);

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("help: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}