// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space head command
 */
#include "user_syscall.h"

static void head_file(const char* path, int lines) {
    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) {
        printf("head: cannot open '%s'\n", path);
        return;
    }

    char buf[4096];
    ssize_t n;
    int line_count = 0;

    while (line_count < lines && (n = sys_read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n && line_count < lines; i++) {
            if (buf[i] == '\n') line_count++;
            sys_write(1, &buf[i], 1);
        }
    }

    sys_close(fd);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    int lines = 10;
    int start = 1;

    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n') {
        if (argc > 2) {
            lines = atoi(argv[2]);
            start = 3;
        } else {
            start = 2;
        }
    }

    if (start >= argc) {
        /* stdin */
        char buf[4096];
        int line_count = 0;
        ssize_t n;
        while (line_count < lines && (n = sys_read(0, buf, sizeof(buf))) > 0) {
            for (int i = 0; i < n && line_count < lines; i++) {
                if (buf[i] == '\n') line_count++;
                sys_write(1, &buf[i], 1);
            }
        }
    } else {
        for (int i = start; i < argc; i++) {
            head_file(argv[i], lines);
        }
    }
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("head: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}