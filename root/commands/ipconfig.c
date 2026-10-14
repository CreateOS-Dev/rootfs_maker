#include "user_syscall.h"

static void print_interface(int index)
{
    netdev_info_t info;
    if (sys_net_info(index, &info) < 0) return;
    printf("%s: DHCP=%s IPv4=%d.%d.%d.%d mask=%d.%d.%d.%d gateway=%d.%d.%d.%d dns=%d.%d.%d.%d\n",
           info.name,
           info.dhcp_state == NETDEV_DHCP_BOUND ? "bound" : "pending",
           info.ipv4_address[0], info.ipv4_address[1], info.ipv4_address[2], info.ipv4_address[3],
           info.ipv4_netmask[0], info.ipv4_netmask[1], info.ipv4_netmask[2], info.ipv4_netmask[3],
           info.ipv4_gateway[0], info.ipv4_gateway[1], info.ipv4_gateway[2], info.ipv4_gateway[3],
           info.ipv4_dns[0], info.ipv4_dns[1], info.ipv4_dns[2], info.ipv4_dns[3]);
}

int main(int argc, char* argv[])
{
    int count;
    if (argc > 1 && strcmp(argv[1], "dhcp") == 0) {
        count = sys_net_count();
        if (count <= 0) {
            sys_puts("ipconfig: no network interfaces\n", 0x0E);
            return 1;
        }
        for (int i = 0; i < count; i++)
            (void)sys_net_dhcp(i);
        for (int i = 0; i < count; i++)
            print_interface(i);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "status") == 0) {
        argc = 1;
    }
    if (argc > 1) {
        sys_puts("Usage: ipconfig [dhcp|status]\n", 0x0E);
        return 2;
    }
    count = sys_net_count();
    if (count <= 0) {
        sys_puts("ipconfig: no network interfaces\n", 0x0E);
        return 0;
    }
    for (int i = 0; i < count; i++)
        print_interface(i);
    return 0;
}
