#include "user_syscall.h"

int main(int argc, char* argv[])
{
    int value = 0;
    int filled;
    if (argc != 2) {
        sys_puts("Usage: progress 0..100\n", 0x0E);
        return 2;
    }
    value = atoi(argv[1]);
    if (value < 0 || value > 100) {
        sys_puts("progress: value must be between 0 and 100\n", 0x0C);
        return 1;
    }
    filled = value / 5;
    sys_puts("[", 0x0B);
    for (int i = 0; i < 20; i++)
        sys_puts(i < filled ? "#" : ".", i < filled ? 0x0A : 0x08);
    printf("] %d%%\n", value);
    return 0;
}
