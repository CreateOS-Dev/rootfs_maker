// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space banner command
 * Built with user_syscall.h for userspace
 */
#include "user_syscall.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    sys_puts("\n", 0x0F);
    sys_puts("   ____ ____  _____    _  _____ ___  ____\n", 0x0B);
    sys_puts("  / ___|  _ \\| ____|  / \\|_   _/ _ \\|  _ \\\n", 0x0B);
    sys_puts(" | |   | |_) |  _|   / _ \\ | || | | | |_) |\n", 0x0B);
    sys_puts(" | |___|  _ <| |___ / ___ \\| || |_| |  _ <\n", 0x0B);
    sys_puts("  \\____|_| \\_\\_____/_/   \\_\\_| \\___/|_| \\_\\\n", 0x0B);
    sys_puts("\n  x86_64  |  CreateOS command environment\n\n", 0x07);
    return 0;
}