// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space stat command - display file status
 * Uses sys_stat syscall
 */
#include "user_syscall.h"

static void print_mode(uint16_t mode) {
    char buf[16];
    int i = 0;

    /* File type */
    buf[i++] = (mode & 0xF000) == 0x4000 ? 'd' : '-';

    /* Owner */
    buf[i++] = (mode & 0x0100) ? 'r' : '-';
    buf[i++] = (mode & 0x0080) ? 'w' : '-';
    buf[i++] = (mode & 0x0040) ? 'x' : '-';

    /* Group */
    buf[i++] = (mode & 0x0020) ? 'r' : '-';
    buf[i++] = (mode & 0x0010) ? 'w' : '-';
    buf[i++] = (mode & 0x0008) ? 'x' : '-';

    /* Others */
    buf[i++] = (mode & 0x0004) ? 'r' : '-';
    buf[i++] = (mode & 0x0002) ? 'w' : '-';
    buf[i++] = (mode & 0x0001) ? 'x' : '-';

    buf[i] = 0;
    sys_puts(buf, 0x0F);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        sys_puts("Usage: stat <file>\n", 0x0C);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        fs_stat_t st;
        if (sys_stat(argv[i], &st) < 0) {
            sys_puts("stat: cannot stat '", 0x0C);
            sys_puts(argv[i], 0x0C);
            sys_puts("': No such file or directory\n", 0x0C);
            continue;
        }

        sys_puts("  File: ", 0x0F);
        sys_puts(argv[i], 0x0F);
        sys_putchar('\n');

        char buf[64];
        sys_puts("  Size: ", 0x0F);
        sprintf(buf, "%u", st.size);
        sys_puts(buf, 0x0F);
        sys_puts("\n", 0x0F);

        sys_puts("  Inode: ", 0x0F);
        sys_puts("0\n", 0x0F);

        sys_puts("  Mode: ", 0x0F);
        print_mode(st.mode);
        sys_putchar('\n');
    }

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("stat: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}