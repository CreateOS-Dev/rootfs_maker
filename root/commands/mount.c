#include "user_syscall.h"

static int equals_ignore_case(const char* left, const char* right)
{
    while (*left && *right) {
        char a = *left;
        char b = *right;
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
        left++;
        right++;
    }
    return *left == *right;
}

static int parse_type(const char* text, fs_type_t* type)
{
    if (!text || !type) return 0;
    if (equals_ignore_case(text, "cfs5")) {
        *type = FS_TYPE_CFS5;
        return 1;
    }
    if (equals_ignore_case(text, "fat32")) {
        *type = FS_TYPE_FAT32;
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    fs_type_t type = FS_TYPE_CFS5;
    int flags = 0;
    int type_seen = 0;

    if (argc < 3 || argc > 5) {
        sys_puts("Usage: mount <device> <mountpoint> [cfs5|fat32] [ro|rw]\n"
                 "Only CFS5 and FAT32 mounts are implemented.\n", 0x0E);
        return 2;
    }
    if (!argv[1][0] || !argv[2][0] ||
        (argv[2][0] != '/' &&
        !(((argv[2][0] >= 'a' && argv[2][0] <= 'z') ||
           (argv[2][0] >= 'A' && argv[2][0] <= 'Z')) &&
          argv[2][1] == ':' && !argv[2][2]))) {
        sys_puts("mount: device and a mount path are required\n", 0x0C);
        return 2;
    }

    for (int i = 3; i < argc; i++) {
        if (equals_ignore_case(argv[i], "ro")) {
            flags |= VFS_MOUNT_READ_ONLY;
        } else if (equals_ignore_case(argv[i], "rw")) {
            flags &= ~VFS_MOUNT_READ_ONLY;
        } else if (!type_seen && parse_type(argv[i], &type)) {
            type_seen = 1;
        } else {
            printf("mount: unsupported type or option '%s'\n", argv[i]);
            return 2;
        }
    }
    if ((flags & VFS_MOUNT_READ_ONLY) && type != FS_TYPE_FAT32) {
        sys_puts("mount: read-only mounting is currently supported for FAT32 only\n",
                 0x0E);
        return 2;
    }

    printf("Mounting %s at %s as %s%s...\n", argv[1], argv[2],
           type == FS_TYPE_FAT32 ? "FAT32" : "CFS5",
           flags & VFS_MOUNT_READ_ONLY ? " (read-only)" : "");
    if (sys_mount(argv[1], argv[2], type, flags, NULL) < 0) {
        sys_puts("mount: operation failed\n", 0x0C);
        return 1;
    }
    sys_puts("Mount successful.\n", 0x0F);
    return 0;
}
