#include "user_syscall.h"

static int print_interface(int index)
{
    netdev_info_t info;
    if (sys_net_info(index, &info) < 0)
        return 0;
    printf("%s: link=%s MAC=", info.name, info.link_up ? "up" : "down");
    for (int i = 0; i < 6; i++) {
        if (i) sys_putchar(':');
        printf("%02x", info.mac[i]);
    }
    printf(" ipv4=%d.%d.%d.%d/%d.%d.%d.%d",
           info.ipv4_address[0], info.ipv4_address[1],
           info.ipv4_address[2], info.ipv4_address[3],
           info.ipv4_netmask[0], info.ipv4_netmask[1],
           info.ipv4_netmask[2], info.ipv4_netmask[3]);
    printf(" gw=%d.%d.%d.%d dns=%d.%d.%d.%d\n",
           info.ipv4_gateway[0], info.ipv4_gateway[1],
           info.ipv4_gateway[2], info.ipv4_gateway[3],
           info.ipv4_dns[0], info.ipv4_dns[1],
           info.ipv4_dns[2], info.ipv4_dns[3]);
    return 1;
}

int main(int argc, char* argv[])
{
    int count;
    if (argc > 1) {
        if (argc == 2 && strcmp(argv[1], "dhcp") == 0) {
            count = sys_net_count();
            if (count <= 0) {
                sys_puts("ifconfig: no network interfaces\n", 0x0E);
                return 1;
            }
            for (int i = 0; i < count; i++) {
                if (sys_net_dhcp(i) >= 0)
                    (void)print_interface(i);
            }
            return 0;
        }
        sys_puts("Usage: ifconfig [dhcp]\n", 0x0E);
        return 2;
    }
    count = sys_net_count();
    if (count <= 0) {
        sys_puts("ifconfig: no network interfaces detected\n", 0x0E);
        return 0;
    }
    for (int i = 0; i < count; i++)
        if (!print_interface(i))
            return 1;
    return 0;
}
