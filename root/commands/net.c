#include "user_syscall.h"

static int parse_index(const char* text, int* index)
{
    unsigned int value = 0;
    if (!text || !text[0] || !index) return 0;
    for (const char* p = text; *p; p++) {
        unsigned int digit;
        if (*p < '0' || *p > '9') return 0;
        digit = (unsigned int)(*p - '0');
        if (value > (2147483647U - digit) / 10U) return 0;
        value = value * 10U + digit;
    }
    *index = (int)value;
    return 1;
}

static void print_mac(const uint8_t mac[6])
{
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        if (i) sys_putchar(':');
        sys_putchar(hex[mac[i] >> 4]);
        sys_putchar(hex[mac[i] & 0x0f]);
    }
}

static void print_ipv4(const uint8_t address[4])
{
    for (int i = 0; i < 4; i++) {
        unsigned int value = address[i];
        char digits[3];
        int count = 0;
        if (i) sys_putchar('.');
        do {
            digits[count++] = (char)('0' + value % 10);
            value /= 10;
        } while (value);
        while (count) sys_putchar(digits[--count]);
    }
}

static void print_ipv6(const uint8_t address[16])
{
    static const char hex[] = "0123456789abcdef";
    for (int group = 0; group < 8; group++) {
        if (group) sys_putchar(':');
        for (int byte = 0; byte < 2; byte++) {
            uint8_t value = address[group * 2 + byte];
            sys_putchar(hex[value >> 4]);
            sys_putchar(hex[value & 0x0f]);
        }
    }
}

static const char* ipv6_state_name(uint32_t state)
{
    switch (state) {
        case NETDEV_IPV6_TENTATIVE: return "duplicate check";
        case NETDEV_IPV6_LINK_LOCAL: return "link-local";
        case NETDEV_IPV6_CONFIGURED: return "SLAAC configured";
        case NETDEV_IPV6_DUPLICATE: return "duplicate address";
        default: return "disabled";
    }
}

static const char* dhcp_state_name(uint32_t state)
{
    switch (state) {
        case NETDEV_DHCP_DISCOVERING: return "discovering";
        case NETDEV_DHCP_REQUESTING: return "requesting";
        case NETDEV_DHCP_PROBING: return "checking address";
        case NETDEV_DHCP_BOUND: return "configured";
        case NETDEV_DHCP_RENEWING: return "renewing";
        case NETDEV_DHCP_FAILED: return "failed";
        case NETDEV_DHCP_CONFLICT: return "address conflict";
        default: return "not configured";
    }
}

static int show_interface(int index, int show_ip)
{
    netdev_info_t info;
    if (sys_net_info(index, &info) < 0) {
        printf("net: unable to query device %d\n", index);
        return 0;
    }
    printf("%s: %s, max frame %d, MAC ", info.name,
           info.link_up ? "link up" : "link down",
           (int)info.max_frame_size);
    print_mac(info.mac);
    printf(" (%s)\n", info.mac_is_local ? "locally generated" : "hardware");
    if (show_ip) {
        printf("  DHCPv4: %s\n  IPv4: ", dhcp_state_name(info.dhcp_state));
        print_ipv4(info.ipv4_address);
        printf("  mask: ");
        print_ipv4(info.ipv4_netmask);
        printf("  gateway: ");
        print_ipv4(info.ipv4_gateway);
        printf("  DNS: ");
        print_ipv4(info.ipv4_dns);
        sys_putchar('\n');
        printf("  IPv6: %s\n  address: ", ipv6_state_name(info.ipv6_state));
        print_ipv6(info.ipv6_address);
        printf("/%d  router: ", (int)info.ipv6_prefix_length);
        print_ipv6(info.ipv6_gateway);
        printf("  DNS: ");
        print_ipv6(info.ipv6_dns);
        sys_putchar('\n');
    }
    return 1;
}

static int run_dhcp(int index)
{
    netdev_info_t info;
    if (sys_net_dhcp(index) < 0) {
        printf("net: cannot start DHCPv4 on interface %d\n", index);
        return 1;
    }
    for (int attempt = 0; attempt < 200; attempt++) {
        if (sys_net_info(index, &info) < 0) {
            printf("net: unable to query interface %d\n", index);
            return 1;
        }
        if (info.dhcp_state == NETDEV_DHCP_BOUND) {
            (void)show_interface(index, 1);
            return 0;
        }
        if (info.dhcp_state == NETDEV_DHCP_FAILED ||
            info.dhcp_state == NETDEV_DHCP_CONFLICT) {
            (void)show_interface(index, 1);
            return 1;
        }
        (void)sys_sleep(100);
    }
    (void)show_interface(index, 1);
    sys_puts("net: DHCPv4 timed out while waiting for a lease\n", 0x0E);
    return 1;
}

int main(int argc, char* argv[])
{
    int count;

    if (argc == 2 && strcmp(argv[1], "dhcp") == 0) {
        count = sys_net_count();
        if (count <= 0) {
            sys_puts("net: no network devices registered\n", 0x0E);
            return count < 0 ? 1 : 0;
        }
        if (count != 1) {
            sys_puts("Usage: net dhcp <interface-index>\n", 0x0E);
            return 2;
        }
        return run_dhcp(0);
    }
    if (argc == 3 && strcmp(argv[1], "dhcp") == 0) {
        int index;
        if (!parse_index(argv[2], &index) ||
            index >= sys_net_count()) {
            sys_puts("net: invalid interface index\n", 0x0C);
            return 2;
        }
        return run_dhcp(index);
    }
    if (argc > 1) {
        sys_puts("Usage: net [dhcp [interface-index]]\n", 0x0E);
        return 2;
    }
    count = sys_net_count();
    if (count < 0) {
        sys_puts("net: unable to query network devices\n", 0x0C);
        return 1;
    }
    if (!count) {
        sys_puts("net: no network devices registered\n", 0x0E);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        if (!show_interface(i, 1)) return 1;
    }
    return 0;
}
