#include "user_syscall.h"

static void print_u64(uint64_t value)
{
    char digits[20];
    unsigned int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value && count < sizeof(digits));
    while (count)
        sys_putchar(digits[--count]);
}

int main(int argc, char* argv[])
{
    int64_t nanoseconds;
    (void)argv;
    if (argc != 1) {
        sys_puts("Usage: clock\n", 0x0E);
        return 2;
    }
    nanoseconds = sys_monotonic_ns();
    if (nanoseconds < 0) {
        sys_puts("clock: monotonic clock unavailable\n", 0x0C);
        return 1;
    }
    printf("Monotonic time: ");
    print_u64((uint64_t)nanoseconds / 1000000ULL);
    sys_puts(" ms\n", 0x0F);
    return 0;
}
