#include "user_syscall.h"

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        sys_puts(argv[i], 0x0F);
        if (i + 1 < argc) sys_putchar(' ');
        sys_beep(660 + (i % 4) * 110, 45);
    }
    sys_putchar('\n');
    return 0;
}
