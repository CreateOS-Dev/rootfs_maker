// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space initfs command - initialize filesystem
 * Uses syscalls to create initial directory structure
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    sys_puts("initfs: initializing filesystem...\n", 0x0F);

    /* Create standard directories */
    const char* dirs[] = {
        "/bin", "/sbin", "/usr/bin", "/usr/sbin",
        "/etc", "/home", "/root", "/tmp", "/var", "/mnt", "/media",
        "/lib", "/usr/lib", "/boot", "/dev", "/proc", "/sys",
        NULL
    };

    for (int i = 0; dirs[i]; i++) {
        if (sys_mkdir(dirs[i], 0755) < 0) {
            sys_puts("initfs: warning: failed to create ", 0x0E);
            sys_puts(dirs[i], 0x0E);
            sys_putchar('\n');
        }
    }

    /* Create user home directories */
    sys_mkdir("/home/root", 0755);
    sys_mkdir("/home/admin", 0755);
    sys_mkdir("/home/user", 0755);

    sys_puts("initfs: filesystem initialized\n", 0x0A);
    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("initfs: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}