// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space chmod command
 */
#include "user_syscall.h"

static int parse_octal_mode(const char* str, int* mode)
{
    int value = 0;
    if (!str || !str[0]) return 0;
    if (str[0] == '0' && str[1]) str++;
    for (; *str; str++) {
        int digit;
        if (*str < '0' || *str > '7') return 0;
        digit = *str - '0';
        if (value > (0777 - digit) / 8) return 0;
        value = value * 8 + digit;
    }
    *mode = value;
    return 1;
}

static int apply_symbolic_execute_mode(const char* expression,
                                       unsigned int current,
                                       unsigned int* updated)
{
    const char* cursor = expression;
    unsigned int mode = current;

    if (!expression || !updated) return 0;
    while (*cursor) {
        unsigned int bits = 0;
        const char* clause_start = cursor;
        char action;

        while (*cursor == 'u' || *cursor == 'g' ||
               *cursor == 'o' || *cursor == 'a') {
            if (*cursor == 'u' || *cursor == 'a') bits |= 0100;
            if (*cursor == 'g' || *cursor == 'a') bits |= 0010;
            if (*cursor == 'o' || *cursor == 'a') bits |= 0001;
            cursor++;
        }
        if (cursor == clause_start)
            bits = 0111;
        if (*cursor != '+' && *cursor != '-') return 0;
        action = *cursor++;
        if (*cursor != 'x') return 0;
        cursor++;
        if (*cursor && *cursor != ',') return 0;

        if (action == '+')
            mode |= bits;
        else
            mode &= ~bits;
        if (*cursor == ',') {
            cursor++;
            if (!*cursor) return 0;
        }
    }

    *updated = mode;
    return 1;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (argc < 3) {
        sys_puts("Usage: chmod <mode> <file>...\n", 0x0C);
        return 1;
    }

    int ret = 0;

    for (int i = 2; i < argc; i++) {
        int mode;
        fs_stat_t status;
        unsigned int updated;
        if (parse_octal_mode(argv[1], &mode)) {
            updated = (unsigned int)mode;
        } else if (sys_stat(argv[i], &status) == 0 &&
                   apply_symbolic_execute_mode(argv[1], status.mode & 0777,
                                               &updated)) {
            mode = (int)updated;
        } else {
            printf("chmod: invalid mode or inaccessible file '%s'\n",
                   argv[i]);
            ret = 1;
            continue;
        }
        if (sys_chmod(argv[i], mode) < 0) {
            printf("chmod: cannot access '%s'\n", argv[i]);
            ret = 1;
        }
    }
    return ret;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("chmod: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}