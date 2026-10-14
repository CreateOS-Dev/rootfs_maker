#include "user_syscall.h"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("Usage: font 0|1\n");
        return 2;
    }
    int font = atoi(argv[1]);
    if (font < 0 || font > 1 || sys_font_set(font) < 0) {
        sys_puts("font: choose 0 (8x8) or 1 (5x7)\n", 0x0C);
        return 1;
    }
    printf("font: %s\n", font == 0 ? "8x8" : "5x7");
    return 0;
}
