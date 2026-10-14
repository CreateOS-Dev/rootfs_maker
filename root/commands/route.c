#include "user_syscall.h"

int main(int argc, char* argv[])
{
    int count = sys_net_count();
    if (argc > 1 && strcmp(argv[1], "show") == 0) {
        argc = 1;
    }
    if (argc > 1) {
        sys_puts("Usage: route [show]\n", 0x0E);
        return 2;
    }
    if (count <= 0) {
        sys_puts("route: no network interfaces\n", 0x0E);
        return 0;
    }
    sys_puts("Destination     Gateway         Interface\n", 0x0E);
    for (int i = 0; i < count; i++) {
        netdev_info_t info;
        if (sys_net_info(i, &info) < 0) continue;
        printf("default        %d.%d.%d.%d     %s\n",
               info.ipv4_gateway[0], info.ipv4_gateway[1],
               info.ipv4_gateway[2], info.ipv4_gateway[3],
               info.name);
    }
    return 0;
}
