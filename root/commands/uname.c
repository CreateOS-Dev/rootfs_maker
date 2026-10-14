// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "user_syscall.h"

static void print_field(const char* value, int* first)
{
    if (!*first) sys_putchar(' ');
    sys_puts(value, 0x0F);
    *first = 0;
}

int main(int argc, char* argv[])
{
    createos_kernel_version_t version;
    int show_all = 0;
    int show_kernel = 0;
    int show_sysname = 0;
    int show_release = 0;
    int show_version = 0;
    int show_machine = 0;
    int show_nodename = 0;
    int first = 1;

    if (sys_kernel_version(&version) < 0 ||
        version.abi_version != CREATEOS_VERSION_ABI) {
        sys_puts("uname: kernel version information unavailable\n", 0x0C);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || !argv[i][1]) {
            sys_puts("Usage: uname [-asrvmonk]\n", 0x0E);
            return 2;
        }
        for (int j = 1; argv[i][j]; j++) {
            switch (argv[i][j]) {
                case 'a': show_all = 1; break;
                case 's': show_sysname = 1; break;
                case 'r': show_release = 1; break;
                case 'v': show_version = 1; break;
                case 'm': show_machine = 1; break;
                case 'n': show_nodename = 1; break;
                case 'o':
                case 'k': show_kernel = 1; break;
                default:
                    sys_puts("Usage: uname [-asrvmonk]\n", 0x0E);
                    return 2;
            }
        }
    }
    if (!show_all && !show_sysname && !show_release && !show_version &&
        !show_machine && !show_nodename && !show_kernel)
        show_sysname = 1;

    if (show_all || show_sysname)
        print_field(version.name, &first);
    if (show_all || show_nodename)
        print_field("localhost", &first);
    if (show_all || show_release)
        print_field(version.release, &first);
    if (show_all || show_version)
        print_field(version.build_date, &first);
    if (show_all || show_machine)
        print_field(version.architecture, &first);
    sys_putchar('\n');

    if (show_kernel) {
        printf("Kernel: %s %s (%s), built %s\n",
               version.name, version.release, version.codename,
               version.build_date);
        printf("Architecture: %s\n", version.architecture);
        printf("Version ABI: %u\n", version.abi_version);
    }
    return 0;
}

void _start(void)
{
    sys_puts("uname: command requires the CreateOS command runtime\n", 0x0C);
    sys_exit(1);
}