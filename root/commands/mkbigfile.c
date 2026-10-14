// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space mkbigfile command - create large test file
 * Uses sys_open, sys_write syscalls
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        sys_puts("Usage: mkbigfile <path> <size>\n", 0x0C);
        sys_puts("Size in bytes (e.g. 1024)\n", 0x0C);
        return 1;
    }

    const char* path = argv[1];
    int size = atoi(argv[2]);

    if (size <= 0 || size > 1024 * 1024) {
        sys_puts("mkbigfile: invalid size\n", 0x0C);
        return 1;
    }

    int fd = sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        sys_puts("mkbigfile: cannot create file\n", 0x0C);
        return 1;
    }

    uint8_t buf[512];
    for (int i = 0; i < 512; i++) buf[i] = i % 256;

    int written = 0;
    while (written < size) {
        int to_write = (size - written > 512) ? 512 : (size - written);
        int n = sys_write(fd, buf, to_write);
        if (n <= 0) {
            sys_puts("mkbigfile: write error\n", 0x0C);
            sys_close(fd);
            return 1;
        }
        written += n;
    }

    sys_close(fd);

    sys_puts("mkbigfile: created ", 0x0A);
    char buf_num[32];
    sprintf(buf_num, "%d", written);
    sys_puts(buf_num, 0x0A);
    sys_puts(" bytes\n", 0x0A);

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("mkbigfile: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}