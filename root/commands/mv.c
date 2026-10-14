// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space mv command - moves/renames files via copy+unlink
 * Uses sys_open, sys_read, sys_write, sys_unlink
 */
#include "user_syscall.h"

static char* strrchr_local(const char* s, int c) {
    char* last = 0;
    while (*s) {
        if (*s == c) last = (char*)s;
        s++;
    }
    return last;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        sys_puts("Usage: mv <source> <dest>\n", 0x0C);
        return 1;
    }

    /* Try to stat the source - if it fails, try copy approach */
    fs_stat_t st;
    if (sys_stat(argv[1], &st) < 0) {
        printf("mv: cannot stat '%s'\n", argv[1]);
        return 1;
    }

    /* For now, use copy + unlink approach */
    int src_fd = sys_open(argv[1], O_RDONLY, 0);
    if (src_fd < 0) {
        printf("mv: cannot open '%s'\n", argv[1]);
        return 1;
    }

    /* Check if destination is a directory */
    fs_stat_t dst_st;
    int is_dir = 0;
    if (sys_stat(argv[2], &dst_st) == 0 && (dst_st.type == FS_TYPE_DIR)) {
        is_dir = 1;
    }

    char dest_path[256];
    if (is_dir) {
        /* Get basename of source */
        const char* base = strrchr_local(argv[1], '/');
        if (base) base++; else base = argv[1];
        snprintf(dest_path, sizeof(dest_path), "%s/%s", argv[2], base);
    } else {
        strncpy(dest_path, argv[2], sizeof(dest_path) - 1);
        dest_path[sizeof(dest_path) - 1] = 0;
    }

    int dst_fd = sys_open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, st.mode);
    if (dst_fd < 0) {
        sys_close(src_fd);
        printf("mv: cannot create '%s'\n", dest_path);
        return 1;
    }

    char buf[4096];
    ssize_t n;
    while ((n = sys_read(src_fd, buf, sizeof(buf))) > 0) {
        if (sys_write(dst_fd, buf, n) != n) {
            sys_puts("mv: write error\n", 0x0C);
            sys_close(src_fd);
            sys_close(dst_fd);
            sys_unlink(dest_path);
            return 1;
        }
    }

    sys_close(src_fd);
    sys_close(dst_fd);

    if (n < 0) {
        sys_puts("mv: read error\n", 0x0C);
        sys_unlink(dest_path);
        return 1;
    }

    /* Unlink the source */
    if (sys_unlink(argv[1]) < 0) {
        sys_puts("mv: failed to remove source\n", 0x0C);
        return 1;
    }

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("mv: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}