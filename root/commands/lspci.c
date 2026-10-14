#include "user_syscall.h"

static const char* class_name(uint8_t class_code)
{
    switch (class_code) {
    case 0x01: return "mass storage";
    case 0x02: return "network";
    case 0x03: return "display";
    case 0x04: return "multimedia";
    case 0x05: return "memory";
    case 0x06: return "bridge";
    case 0x07: return "communication";
    case 0x08: return "system peripheral";
    case 0x09: return "input";
    case 0x0A: return "docking station";
    case 0x0B: return "processor";
    case 0x0C: return "serial bus";
    case 0x0D: return "wireless";
    case 0x0E: return "intelligent I/O";
    case 0x0F: return "satellite communication";
    case 0x10: return "encryption";
    case 0x11: return "signal processing";
    case 0x12: return "processing accelerator";
    case 0x13: return "non-essential instrumentation";
    default: return "unclassified";
    }
}

static void print_hex(uint32_t value, unsigned int digits)
{
    static const char hex[] = "0123456789abcdef";
    while (digits) {
        digits--;
        sys_putchar(hex[(value >> (digits * 4)) & 0x0f]);
    }
}

static int print_devices(void)
{
    int count = sys_pci_info(0, 0, 0);
    if (count < 0) {
        sys_puts("lspci: unable to enumerate PCI devices\n", 0x0C);
        return 1;
    }
    if (count == 0) {
        sys_puts("lspci: no PCI devices found\n", 0x0E);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        pci_info_t info;
        if (sys_pci_info(1, i, &info) < 0) {
            printf("lspci: failed to read PCI device %d\n", i);
            return 1;
        }
        print_hex(info.bus, 2);
        sys_putchar(':');
        print_hex(info.slot, 2);
        sys_putchar('.');
        print_hex(info.function, 1);
        sys_putchar(' ');
        print_hex(info.vendor, 4);
        sys_putchar(':');
        print_hex(info.device, 4);
        sys_puts(" class ", 0x0F);
        print_hex(info.class_code, 2);
        print_hex(info.subclass, 2);
        printf(": %s (prog-if ", class_name(info.class_code));
        print_hex(info.prog_if, 2);
        sys_puts(")\n", 0x0F);
    }
    return 0;
}

int main(int argc, char* argv[])
{
    (void)argv;
    if (argc != 1) {
        sys_puts("Usage: lspci\n", 0x0E);
        return 2;
    }
    return print_devices();
}
