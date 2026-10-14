// ============================================================
// CreateOS 0.0.1
// ============================================================

/* Authenticated root shell shortcut. */
#include "user_syscall.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    char* sudo_argv[] = { "/bin/sudo", "-i", NULL };
    sys_exec("/bin/sudo", sudo_argv);
    sys_puts("goddo: unable to start authenticated root shell\n", 0x0C);
    return 1;
}

void _start(void)
{
    (void)main(0, (char**)0);
    sys_exit(1);
}