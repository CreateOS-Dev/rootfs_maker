// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "user_syscall.h"
#include "network_startup.h"

static void launch_or_retry(const char* path, char* const argv[])
{
    sys_puts("[init] Starting ", 0x0F);
    sys_puts(path, 0x0F);
    sys_puts("\n", 0x0F);

    if (sys_exec(path, argv) >= 0) {
        sys_exit(0);
    }

    sys_puts("[init] FATAL: Failed to execute ", 0x0C);
    sys_puts(path, 0x0C);
    sys_puts("\n", 0x0C);
}

void _start(void) {
    fs_stat_t marker;

    if (sys_user_count() == 0 &&
        sys_stat("/etc/.firstboot_done", &marker) < 0) {
        char* setup_argv[] = { "/bin/firstboot", NULL };
        launch_or_retry("/bin/firstboot", setup_argv);
    }

    network_startup_run();

    {
        char* login_argv[] = { "/bin/login", NULL };
        launch_or_retry("/bin/login", login_argv);
    }

    for (;;) sys_sleep(1000);
}