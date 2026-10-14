#include "user_syscall.h"

static void print_uint(uint32_t value) {
    char digits[10];
    int length = 0;
    do {
        digits[length++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (length > 0)
        sys_putchar(digits[--length]);
}

static int list_consoles(void) {
    char current[16] = { 0 };
    if (sys_tty_name(current) < 0) {
        sys_puts("tty: unable to query the active console\n", 0x0C);
        return 1;
    }

    sys_puts("Current console: ", 0x0E);
    sys_puts(current, 0x0E);
    sys_putchar('\n');

    for (int index = 0; index < 5; index++) {
        tty_info_t info;
        if (sys_tty_info(index, &info) < 0) {
            sys_puts("tty: unable to query console information\n", 0x0C);
            return 1;
        }
        sys_puts("  ", 0x07);
        sys_puts(info.name, 0x0F);
        sys_puts(" ", 0x07);
        print_uint(info.width);
        sys_putchar('x');
        print_uint(info.height);
        if (info.flags & TTY_FLAG_GRAPHICAL)
            sys_puts(" graphical", 0x0B);
        if (info.flags & TTY_FLAG_ACTIVE)
            sys_puts(" active", 0x0A);
        sys_putchar('\n');
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "list") == 0))
        return list_consoles();
    if (argc != 2) {
        sys_puts("Usage: tty [list | tty1..tty5]\n", 0x0C);
        return 2;
    }

    if (sys_tty_activate(argv[1]) < 0) {
        sys_puts("tty: unknown console: ", 0x0C);
        sys_puts(argv[1], 0x0C);
        sys_putchar('\n');
        return 2;
    }

    char current[16] = { 0 };
    if (sys_tty_name(current) < 0) {
        sys_puts("tty: unable to verify the active console\n", 0x0C);
        return 1;
    }
    if (strcmp(current, argv[1]) != 0) {
        sys_puts("tty: console switch is unavailable on this output device\n", 0x0C);
        return 1;
    }

    sys_puts("Active console: ", 0x0A);
    sys_puts(current, 0x0A);
    sys_putchar('\n');
    return 0;
}
