#include "user_syscall.h"

int main(int argc, char* argv[])
{
    (void)argv;
    if (argc != 1) {
        sys_puts("Usage: sync\n", 0x0E);
        return 2;
    }
    if (sys_sync() < 0) {
        sys_puts("sync: storage flush failed\n", 0x0C);
        return 1;
    }
    sys_puts("sync: storage cache flushed\n", 0x0A);
    return 0;
}
