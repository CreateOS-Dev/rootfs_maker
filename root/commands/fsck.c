// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "user_syscall.h"

int main(int argc, char* argv[])
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "/") != 0)) {
        sys_puts("fsck: checks the mounted CFS5 root; usage: fsck [/]\n", 0x0C);
        return 2;
    }

    sys_puts("fsck: checking mounted CFS5 root (read-only)...\n", 0x0F);
    if (sys_fsck() < 0) {
        sys_puts("fsck: failed (permission denied, unavailable filesystem, or detected errors)\n",
                 0x0C);
        return 1;
    }

    sys_puts("fsck: superblock, inode allocation, extents, and directory entries are valid\n",
             0x0A);
    sys_puts("fsck: no repairs were made\n", 0x0F);
    return 0;
}
