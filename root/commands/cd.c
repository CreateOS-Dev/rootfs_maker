// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space cd command - change directory
 * Uses syscalls via user_syscall.h
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    if (argc > 2) {
        printf("Usage: cd [directory]\n");
        return 2;
    }
    const char* path = argc == 2 ? argv[1] : "/";
    if (sys_chdir(path) < 0) {
        printf("cd: cannot change directory to %s\n", path);
        return 1;
    }
    return 0;
}