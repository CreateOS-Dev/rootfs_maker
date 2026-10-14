// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space echo command with color support
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    int color = 0x0F;
    int start = 1;

    if (argc > 1 && argv[1][0] == '-') {
        switch (argv[1][1]) {
            case 'r': case 'R': color = 0x0C; break;
            case 'g': case 'G': color = 0x0A; break;
            case 'b': case 'B': color = 0x09; break;
        }
        start = 2;
    }

    for (int i = start; i < argc; i++) {
        sys_puts(argv[i], color);
        if (i < argc - 1) sys_putchar(' ');
    }
    sys_putchar('\n');
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("echo: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}