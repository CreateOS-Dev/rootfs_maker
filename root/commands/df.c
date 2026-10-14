// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space df command - disk free space via sys_statfs syscall
 * Gets real filesystem information from kernel
 * Supports drive letters (C:, D:, etc.) and paths
 */
#include "user_syscall.h"
#include "../version.h"
#include <string.h>
#include <ctype.h>

/* Check if a string is a drive letter (e.g., "C:", "D:") */
static int is_drive_letter(const char* str) {
    return str && isalpha(str[0]) && str[1] == ':' && str[2] == '\0';
}

/* Convert drive letter to path for statfs (e.g., "C:" -> "C:/") */
static void drive_letter_to_path(char* dest, size_t size, const char* drive_letter) {
    if (drive_letter && strlen(drive_letter) == 2 && isalpha(drive_letter[0]) && drive_letter[1] == ':') {
        snprintf(dest, size, "%c:/", toupper(drive_letter[0]));
    } else {
        strncpy(dest, drive_letter, size - 1);
        dest[size - 1] = '\0';
    }
}

int main(int argc, char* argv[]) {
    char path[256];

    sys_puts("Filesystem     1K-blocks    Used Available Use% Mounted on\n", 0x0F);

    // If argument provided, check if it specified
    if (argc > 1) {
        // Convert drive letter to path if needed
        if (is_drive_letter(argv[1])) {
            drive_letter_to_path(path, sizeof(path), argv[1]);
        } else {
            strncpy(path, argv[1], sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
        }

        statfs_t fs;
        if (sys_statfs(path, &fs) == 0) {
            uint64_t bsize_kb = fs.f_bsize / 1024;
            uint64_t total = fs.f_blocks * bsize_kb;
            uint64_t free = fs.f_bfree * bsize_kb;
            uint64_t used = total - free;
            uint64_t avail = fs.f_bavail * bsize_kb;
            uint64_t use_pct = total ? (used * 100) / total : 0;

            printf("%-12s %10llu %7llu %9llu %3llu%% %s\n",
                   "unknown", total, used, avail, use_pct, path);
        } else {
            printf("%-12s %10s %7s %9s %3s%% %s\n",
                   "unknown", "N/A", "N/A", "N/A", "N/A", path);
            sys_puts(" (failed to get stats)\n", 0x0C);
        }
    } else {
        // No argument - try to show mounted filesystems
        // First try root filesystem
        statfs_t fs;
        if (sys_statfs("/", &fs) == 0) {
            uint64_t bsize_kb = fs.f_bsize / 1024;
            uint64_t total = fs.f_blocks * bsize_kb;
            uint64_t free = fs.f_bfree * bsize_kb;
            uint64_t used = total - free;
            uint64_t avail = fs.f_bavail * bsize_kb;
            uint64_t use_pct = total ? (used * 100) / total : 0;

            printf("/dev/cfs5   %10llu %7llu %9llu %3llu%% /\n",
                   total, used, avail, use_pct);
        }

        // Try common drive letters A-Z
        for (char drive = 'A'; drive <= 'Z'; drive++) {
            char drive_path[8];
            snprintf(drive_path, sizeof(drive_path), "%c:/", drive);

            // Check if this drive is mounted by trying to stat it
            if (sys_statfs(drive_path, &fs) == 0) {
                uint64_t bsize_kb = fs.f_bsize / 1024;
                uint64_t total = fs.f_blocks * bsize_kb;
                uint64_t free = fs.f_bfree * bsize_kb;
                uint64_t used = total - free;
                uint64_t avail = fs.f_bavail * bsize_kb;
                uint64_t use_pct = total ? (used * 100) / total : 0;

                printf("%c:         %10llu %7llu %9llu %3llu%% %c:\n",
                       drive, total, used, avail, use_pct, drive);
            }
        }
    }

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("df: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}