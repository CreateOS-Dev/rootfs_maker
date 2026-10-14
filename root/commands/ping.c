#include "user_syscall.h"

static int parse_count(const char* text, int* count)
{
    unsigned int value = 0;
    if (!text || !text[0] || !count) return 0;
    for (const char* p = text; *p; p++) {
        unsigned int digit;
        if (*p < '0' || *p > '9') return 0;
        digit = (unsigned int)(*p - '0');
        if (value > (10U - digit) / 10U) return 0;
        value = value * 10U + digit;
    }
    if (value < 1 || value > 10) return 0;
    *count = (int)value;
    return 1;
}

static int parse_ipv4(const char* text, uint8_t address[4])
{
    const char* p = text;
    if (!text || !text[0]) return 0;
    for (int part = 0; part < 4; part++) {
        unsigned int value = 0;
        unsigned int digits = 0;
        while (*p >= '0' && *p <= '9') {
            unsigned int digit = (unsigned int)(*p++ - '0');
            if (value > (255U - digit) / 10U) return 0;
            value = value * 10U + digit;
            digits++;
        }
        if (!digits) return 0;
        address[part] = (uint8_t)value;
        if (part < 3) {
            if (*p++ != '.') return 0;
        } else if (*p) {
            return 0;
        }
    }
    return 1;
}

static int select_interface(void)
{
    int count = sys_net_count();
    if (count < 0) return -2;
    for (int i = 0; i < count; i++) {
        netdev_info_t info;
        if (sys_net_info(i, &info) == 0 && info.link_up &&
            info.dhcp_state == NETDEV_DHCP_BOUND)
            return i;
    }
    return -1;
}

int main(int argc, char* argv[])
{
    uint8_t address[4];
    const char* destination;
    int count = 4;
    int interface_index;
    int transmitted = 0;
    int received = 0;

    if (argc == 2) {
        destination = argv[1];
    } else if (argc == 4 && strcmp(argv[1], "-c") == 0 &&
               parse_count(argv[2], &count)) {
        destination = argv[3];
    } else {
        sys_puts("Usage: ping [-c count (1-10)] IPv4-address\n", 0x0E);
        return 2;
    }
    if (!parse_ipv4(destination, address)) {
        sys_puts("ping: expected a numeric IPv4 address\n", 0x0C);
        return 2;
    }

    interface_index = select_interface();
    if (interface_index == -2) {
        sys_puts("ping: unable to query network interfaces\n", 0x0C);
        return 1;
    }
    if (interface_index < 0) {
        sys_puts("ping: no link-up interface with a DHCPv4 address\n", 0x0E);
        return 1;
    }

    printf("PING %s: 16 data bytes\n", destination);
    for (int sequence = 0; sequence < count; sequence++) {
        int result = sys_net_ping(interface_index, address, 1000);
        transmitted++;
        if (result < 0) {
            sys_puts("ping: network request failed\n", 0x0C);
            return 1;
        }
        if (result > 0) {
            received++;
            printf("16 bytes from %s: icmp_seq=%d time=%d ms\n",
                   destination, sequence + 1, result - 1);
        } else {
            printf("Request timeout for icmp_seq %d\n", sequence + 1);
        }
        if (sequence + 1 < count) (void)sys_sleep(1000);
    }
    printf("--- %s ping statistics ---\n%d packets transmitted, %d received\n",
           destination, transmitted, received);
    return received ? 0 : 1;
}
