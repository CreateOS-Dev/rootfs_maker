#include "user_syscall.h"

#define NETWORK_CONFIG_PATH "/etc/network.conf"
#define NETWORK_WAIT_ATTEMPTS 50
#define NETWORK_WAIT_MS 100

static int parse_network_config(const char* data, size_t length)
{
    size_t offset = 0;
    int autostart = -1;

    while (offset < length) {
        char line[32];
        size_t line_length = 0;
        while (offset < length && (data[offset] == '\n' || data[offset] == '\r'))
            offset++;
        while (offset < length &&
               (data[offset] == ' ' || data[offset] == '\t'))
            offset++;
        if (offset < length &&
            (data[offset] == '\n' || data[offset] == '\r')) {
            offset++;
            continue;
        }
        if (offset == length || data[offset] == '#') {
            while (offset < length && data[offset] != '\n')
                offset++;
            continue;
        }
        while (offset < length && data[offset] != '\n' &&
               data[offset] != '\r') {
            if (line_length + 1 >= sizeof(line))
                return -1;
            line[line_length++] = data[offset++];
        }
        while (line_length &&
               (line[line_length - 1] == ' ' || line[line_length - 1] == '\t'))
            line_length--;
        line[line_length] = '\0';
        if (autostart >= 0)
            return -1;
        if (strcmp(line, "autostart=1") == 0)
            autostart = 1;
        else if (strcmp(line, "autostart=0") == 0)
            autostart = 0;
        else
            return -1;
    }
    return autostart;
}

static int network_autostart_enabled(void)
{
    char config[129];
    int fd = sys_open(NETWORK_CONFIG_PATH, O_RDONLY, 0);
    ssize_t count;
    int enabled;
    if (fd < 0) {
        sys_puts("[init] Network config unavailable; autostarting DHCP\n",
                 0x0E);
        return 1;
    }
    count = sys_read(fd, config, sizeof(config) - 1);
    if (count < 0) {
        sys_puts("[init] ERROR: failed to read network config; DHCP disabled\n",
                 0x0C);
        (void)sys_close(fd);
        return 0;
    }
    config[count] = '\0';
    if (count == (ssize_t)(sizeof(config) - 1)) {
        char extra;
        ssize_t extra_count = sys_read(fd, &extra, 1);
        if (extra_count != 0) {
            sys_puts("[init] ERROR: network config is too large; DHCP disabled\n",
                     0x0C);
            (void)sys_close(fd);
            return 0;
        }
    }
    if (sys_close(fd) < 0) {
        sys_puts("[init] ERROR: failed to close network config; DHCP disabled\n",
                 0x0C);
        return 0;
    }
    enabled = parse_network_config(config, (size_t)count);
    if (enabled < 0) {
        sys_puts("[init] ERROR: invalid network config; DHCP disabled\n",
                 0x0C);
        return 0;
    }
    return enabled;
}

static void print_ipv4(const uint8_t address[4])
{
    for (int part = 0; part < 4; part++) {
        unsigned int value = address[part];
        char digits[3];
        int count = 0;
        if (part)
            sys_putchar('.');
        do {
            digits[count++] = (char)('0' + value % 10);
            value /= 10;
        } while (value);
        while (count)
            sys_putchar(digits[--count]);
    }
}

static void wait_for_dhcp(int count, int active[], int* pending)
{
    for (int attempt = 0; attempt < NETWORK_WAIT_ATTEMPTS && *pending;
         attempt++) {
        for (int index = 0; index < count; index++) {
            netdev_info_t info;
            if (!active[index])
                continue;
            if (sys_net_info(index, &info) < 0) {
                sys_puts("\n[init] ERROR: network status query failed\n", 0x0C);
                active[index] = 0;
                (*pending)--;
                continue;
            }
            if (info.dhcp_state == NETDEV_DHCP_BOUND) {
                sys_puts("\n[init] Network configured: ", 0x0A);
                print_ipv4(info.ipv4_address);
                sys_putchar('\n');
                active[index] = 0;
                (*pending)--;
            } else if (info.dhcp_state == NETDEV_DHCP_FAILED ||
                       info.dhcp_state == NETDEV_DHCP_CONFLICT) {
                sys_puts("\n[init] WARNING: DHCP failed on network interface\n",
                         0x0E);
                active[index] = 0;
                (*pending)--;
            }
        }
        if (*pending) {
            sys_putchar('.');
            (void)sys_sleep(NETWORK_WAIT_MS);
        }
    }
}

void network_startup_run(void)
{
    int count;
    int active[NETDEV_MAX_DEVICES] = { 0 };
    int pending = 0;

    if (!network_autostart_enabled()) {
        sys_puts("[init] Network autostart disabled by config\n", 0x0E);
        return;
    }
    count = sys_net_count();
    if (count < 0) {
        sys_puts("[init] ERROR: unable to query network devices\n", 0x0C);
        return;
    }
    if (count > NETDEV_MAX_DEVICES) {
        sys_puts("[init] ERROR: network device count exceeds supported limit\n",
                 0x0C);
        count = NETDEV_MAX_DEVICES;
    }
    if (!count) {
        sys_puts("[init] No network devices; continuing without DHCP\n", 0x0E);
        return;
    }

    for (int index = 0; index < count; index++) {
        netdev_info_t info;
        if (sys_net_info(index, &info) < 0) {
            sys_puts("[init] ERROR: unable to inspect network interface\n",
                     0x0C);
            continue;
        }
        if (!info.link_up) {
            sys_puts("[init] Network interface is down; DHCP not started\n",
                     0x0E);
            continue;
        }
        if (info.dhcp_state == NETDEV_DHCP_BOUND)
            continue;
        if ((info.dhcp_state == NETDEV_DHCP_IDLE ||
             info.dhcp_state == NETDEV_DHCP_FAILED ||
             info.dhcp_state == NETDEV_DHCP_CONFLICT) &&
            sys_net_dhcp(index) <= 0) {
            sys_puts("[init] ERROR: could not start DHCP on interface\n", 0x0C);
            continue;
        }
        active[index] = 1;
        pending++;
    }
    if (!pending)
        return;

    sys_puts("[init] Waiting for DHCP address", 0x0F);
    wait_for_dhcp(count, active, &pending);
    if (pending) {
        sys_puts("\n[init] DHCP is still pending; restarting discovery\n", 0x0E);
        for (int index = 0; index < count; index++) {
            netdev_info_t info;
            if (!active[index])
                continue;
            if (sys_net_info(index, &info) < 0) {
                sys_puts("[init] ERROR: network status query failed\n", 0x0C);
                active[index] = 0;
                pending--;
            } else if (info.dhcp_state == NETDEV_DHCP_BOUND) {
                sys_puts("[init] Network configured: ", 0x0A);
                print_ipv4(info.ipv4_address);
                sys_putchar('\n');
                active[index] = 0;
                pending--;
            } else if (!info.link_up || sys_net_dhcp(index) <= 0) {
                sys_puts("[init] ERROR: could not retry DHCP on interface\n",
                         0x0C);
                active[index] = 0;
                pending--;
            }
        }
        if (pending) {
            sys_puts("[init] Retrying DHCP", 0x0F);
            wait_for_dhcp(count, active, &pending);
        }
    }
    if (pending)
        sys_puts("\n[init] WARNING: DHCP still pending; use `net dhcp` to retry\n",
                 0x0E);
}
