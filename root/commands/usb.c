// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "user_syscall.h"

static int parse_index(const char* text, int* value)
{
    unsigned int result = 0;
    if (!text || !text[0] || !value) return 0;
    for (const char* p = text; *p; p++) {
        unsigned int digit;
        if (*p < '0' || *p > '9') return 0;
        digit = (unsigned int)(*p - '0');
        if (result > (2147483647U - digit) / 10U) return 0;
        result = result * 10U + digit;
    }
    *value = (int)result;
    return 1;
}

static void print_u64(uint64_t value)
{
    char digits[20];
    int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value && count < (int)sizeof(digits));
    while (count) sys_putchar(digits[--count]);
}

static int show_controller(int index, int show_ports)
{
    usb_info_t info;
    if (sys_usb_info(1, index, 0, &info) < 0) {
        printf("usb: cannot query controller %d\n", index);
        return 0;
    }
    printf("%d: %s: %s, %d port(s)\n", index,
           info.controller_name,
           info.controller_ready ? "initialized" : "unavailable",
           info.port_count);
    if (info.transfer_ready) {
        printf("  storage: %s, ", info.storage_name);
        print_u64(info.storage_sector_count);
        printf(" sectors, %d partition(s)\n", info.partition_count);
    } else if (info.controller_ready) {
        sys_puts("  no supported USB mass-storage device is ready\n", 0x0E);
    }
    if (!show_ports || !info.controller_ready) return 1;

    for (int port = 0; port < info.port_count; port++) {
        if (sys_usb_info(2, index, port, &info) < 0) {
            printf("  port %d: query failed\n", port);
            return 0;
        }
        printf("  port %d: %s\n", port,
               info.connected ? "connected" : "disconnected");
    }
    return 1;
}

static int show_partitions(int controller)
{
    usb_info_t info;
    if (sys_usb_info(1, controller, 0, &info) < 0) {
        printf("usb: cannot query controller %d\n", controller);
        return 0;
    }
    if (!info.transfer_ready) {
        printf("usb: no mass-storage device on controller %d\n", controller);
        return 0;
    }
    if (!info.partition_count) {
        printf("usb: %s has no recognized MBR/GPT partitions\n",
               info.storage_name);
        printf("If this is a FAT32 superfloppy, mount the whole device: "
               "mount %s /mnt fat32\n", info.storage_name);
        return 1;
    }
    for (int i = 0; i < info.partition_count; i++) {
        if (sys_usb_info(3, controller, i, &info) < 0) {
            printf("usb: failed to query partition %d\n", i);
            return 0;
        }
        printf("%s: first LBA ", info.partition_name);
        print_u64(info.partition_first_lba);
        printf(", ");
        print_u64(info.partition_sector_count);
        printf(" sectors\n");
    }
    printf("Mount a supported FAT32 partition with: mount <partition> <mountpoint> fat32\n");
    return 1;
}

int main(int argc, char* argv[])
{
    usb_info_t info;
    int result = 0;

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "list") == 0)) {
        if (sys_usb_info(0, 0, 0, &info) < 0) {
            sys_puts("usb: controller query failed\n", 0x0C);
            return 1;
        }
        if (info.controller_count == 0) {
            sys_puts("usb: no USB controllers detected\n", 0x0E);
            return 0;
        }
        for (int i = 0; i < info.controller_count; i++)
            if (!show_controller(i, 0)) result = 1;
        return result;
    }

    if (argc == 3 && strcmp(argv[1], "ports") == 0) {
        int controller;
        if (!parse_index(argv[2], &controller)) {
            sys_puts("Usage: usb ports <controller-index>\n", 0x0C);
            return 2;
        }
        return show_controller(controller, 1) ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "partitions") == 0) {
        int controller;
        if (!parse_index(argv[2], &controller)) {
            sys_puts("Usage: usb partitions <controller-index>\n", 0x0C);
            return 2;
        }
        return show_partitions(controller) ? 0 : 1;
    }

    sys_puts("Usage: usb [list | ports <controller-index> | partitions <controller-index>]\n",
             0x0E);
    sys_puts("Controller/port indexes are zero-based.\n", 0x0E);
    return 2;
}
