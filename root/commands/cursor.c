#include "user_syscall.h"

static const char* const cursor_themes[] = {
    "classic.srbmp",
    "high-contrast.srbmp",
    "inverted.srbmp",
    "createos.srbmp",
    "blue-arrow.srbmp",
    "red-arrow.srbmp",
    "crosshair.srbmp",
    "precision.srbmp",
    "hand.srbmp"
};

static int theme_is_supported(const char* name)
{
    for (size_t i = 0; i < sizeof(cursor_themes) / sizeof(cursor_themes[0]); i++)
        if (strcmp(name, cursor_themes[i]) == 0) return 1;
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc == 1) {
        char selected[64] = {0};
        ssize_t count = sys_read_at("/etc/cursors/default", selected,
                                    sizeof(selected) - 1, 0);
        if (count < 0) {
            sys_puts("cursor: cannot read the selected theme\n", 0x0C);
            return 1;
        }
        while (count > 0 &&
               (selected[count - 1] == '\n' || selected[count - 1] == '\r'))
            selected[--count] = 0;
        printf("Selected cursor: %s\nAvailable: classic.srbmp, "
               "high-contrast.srbmp, inverted.srbmp, createos.srbmp, "
               "blue-arrow.srbmp, red-arrow.srbmp, crosshair.srbmp, "
               "precision.srbmp, hand.srbmp\n",
               selected);
        return 0;
    }

    if (argc != 2 || !theme_is_supported(argv[1])) {
        sys_puts("Usage: cursor [classic.srbmp|high-contrast.srbmp|"
                 "inverted.srbmp|createos.srbmp|blue-arrow.srbmp|"
                 "red-arrow.srbmp|crosshair.srbmp|precision.srbmp|"
                 "hand.srbmp]\n",
                 0x0E);
        return 2;
    }
    if (sys_file_replace("/etc/cursors/default", argv[1],
                         strlen(argv[1])) < 0) {
        sys_puts("cursor: cannot save selection; administrator access is required\n",
                 0x0C);
        return 1;
    }
    printf("Cursor theme set to %s and applied immediately.\n", argv[1]);
    return 0;
}
