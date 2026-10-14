// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * umount.c — Unmount filesystems
 */
#include "stdio.h"
#include <string.h>
#include "userlib.h"
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: umount <mountpoint>\n");
        printf("  Unmount a filesystem from the specified mount point or drive letter\n");
        printf("  Example: umount C:\n");
        printf("           umount /mnt/data\n");
        return 1;
    }

    const char* mountpoint = argv[1];

    // Validate mountpoint
    if (!mountpoint || (strlen(mountpoint) == 0)) {
        printf("Error: Mount point cannot be empty\n");
        return 1;
    }

    printf("Unmounting %s...\n", mountpoint);

    if (sys_sync() < 0) {
        printf("Unmount refused: storage flush failed.\n");
        return 1;
    }

    int result = sys_unmount(mountpoint); // Using fs_unmount which delegates to vfs_unmount

    if (result == 0) {
        printf("Unmount successful.\n");
        return 0;
    } else {
        printf("Unmount failed with error %d\n", result);
        return 1;
    }
}