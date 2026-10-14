// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space uptime command - shows system uptime
 * Uses sys_ticks syscall
 */
#include "user_syscall.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    unsigned long ticks = sys_ticks();
    unsigned long seconds = ticks / 100;  // Assuming 100Hz timer
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    char buf[64];
    sys_puts("up ", 0x0F);

    if (days > 0) {
        sprintf(buf, "%lu day%s, ", days, days == 1 ? "" : "s");
        sys_puts(buf, 0x0F);
    }
    sprintf(buf, "%02lu:%02lu:%02lu\n", hours, minutes, seconds);
    sys_puts(buf, 0x0F);

    return 0;
}

void _start(void) {
    extern int main(int argc, char* argv[]);
    sys_puts("uptime: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}