#include "user_syscall.h"

static const char* const layouts[] = {
    "us.lyt",
    "fr-azerty.lyt"
};

static int layout_is_supported(const char* name)
{
    for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++)
        if (strcmp(name, layouts[i]) == 0) return 1;
    return 0;
}

static int personal_layout_name_valid(const char* name)
{
    size_t length;
    if (!name) return 0;
    length = strlen(name);
    if (length < 5 || length >= 64 ||
        strcmp(name + length - 4, ".lyt") != 0)
        return 0;
    for (size_t i = 0; i < length - 4; i++) {
        char ch = name[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_'))
            return 0;
    }
    return 1;
}

static int personal_layout_paths(const char* name, char* layout_path,
                                 char* selection_path)
{
    user_t user;
    if (!personal_layout_name_valid(name) ||
        sys_user_current(&user) <= 0)
        return 0;
    if (snprintf(layout_path, 160, "%s/.config/keyboards/%s",
                 user.home, name) >= 160 ||
        snprintf(selection_path, 128, "%s/.config/keyboards/default",
                 user.home) >= 128)
        return 0;
    return 1;
}

int main(int argc, char* argv[])
{
    if (argc == 1) {
        char selected[64] = {0};
        ssize_t count = sys_read_at("/etc/keyboards/default", selected,
                                    sizeof(selected) - 1, 0);
        if (count < 0) {
            sys_puts("keymap: cannot read the selected layout\n", 0x0C);
            return 1;
        }
        while (count > 0 &&
               (selected[count - 1] == '\n' || selected[count - 1] == '\r'))
            selected[--count] = 0;
        printf("Selected keyboard: %s\nAvailable: us.lyt, fr-azerty.lyt\n",
               selected);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "create") == 0) {
        char layout_path[160];
        char selection_path[128];
        char template[2048];
        fs_stat_t status;
        ssize_t size;
        if (!personal_layout_paths(argv[2], layout_path, selection_path)) {
            sys_puts("keymap: invalid layout name or no signed-in user\n", 0x0C);
            return 2;
        }
        if (sys_stat(layout_path, &status) == 0) {
            sys_puts("keymap: layout already exists; edit it in TextEdit\n", 0x0E);
            return 1;
        }
        size = sys_read_path("/etc/keyboards/us.lyt", template,
                             sizeof(template));
        if (size <= 0 || (size_t)size >= sizeof(template) ||
            sys_file_replace(layout_path, template, (size_t)size) < 0) {
            sys_puts("keymap: could not create the layout in your home folder\n",
                     0x0C);
            return 1;
        }
        printf("Created %s. Edit it with TextEdit, then run: keymap use %s\n",
               layout_path, argv[2]);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "use") == 0) {
        char layout_path[160];
        char selection_path[128];
        if (!personal_layout_paths(argv[2], layout_path, selection_path)) {
            sys_puts("keymap: invalid layout name or no signed-in user\n", 0x0C);
            return 2;
        }
        if (sys_stat(layout_path, &(fs_stat_t){0}) < 0 ||
            sys_file_replace(selection_path, argv[2], strlen(argv[2])) < 0) {
            sys_puts("keymap: layout is missing, invalid, or cannot be selected\n",
                     0x0C);
            return 1;
        }
        printf("Personal layout %s saved and applied to this console.\n",
               argv[2]);
        return 0;
    }

    if (argc != 2 || !layout_is_supported(argv[1])) {
        sys_puts("Usage: keymap [us.lyt|fr-azerty.lyt]\n"
                 "       keymap create NAME.lyt\n"
                 "       keymap use NAME.lyt\n", 0x0E);
        return 2;
    }
    if (sys_file_replace("/etc/keyboards/default", argv[1],
                         strlen(argv[1])) < 0) {
        sys_puts("keymap: cannot save selection; administrator access is required\n",
                 0x0C);
        return 1;
    }
    printf("Keyboard layout set to %s and applied immediately.\n", argv[1]);
    return 0;
}
