#include "user_syscall.h"
#include "createos/uikit.h"

#define SETTINGS_COUNT 6

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
static const char* const cursor_types[] = {
    "bar",
    "underline"
};
static const unsigned int suspend_minutes[] = {0, 5, 10, 15, 30};
static int pointer_mode = -1;

static int personal_layout_setting_path(char* path, size_t capacity);

static int set_pointer_mode(int mode)
{
    if (pointer_mode == mode)
        return 0;
    if (sys_tty_pointer_mode(mode) < 0)
        return -1;
    pointer_mode = mode;
    return 0;
}

static int read_setting(const char* path, char* value, size_t capacity)
{
    ssize_t count;
    fs_stat_t status;
    if (!path || !value || capacity < 2) return 0;
    memset(value, 0, capacity);
    if (sys_stat(path, &status) < 0 || status.type != FS_TYPE_FILE ||
        status.size >= capacity)
        return 0;
    count = status.size
        ? sys_read_at(path, value, status.size, 0) : 0;
    if (count < 0 || (uint32_t)count != status.size) return 0;
    value[count] = 0;
    while (count > 0 &&
           (value[count - 1] == '\n' || value[count - 1] == '\r'))
        value[--count] = 0;
    return 1;
}

static int load_keyboard_setting(int* value)
{
    char path[128];
    char text[128];
    fs_stat_t status;

    if (personal_layout_setting_path(path, sizeof(path)) &&
        sys_stat(path, &status) == 0) {
        if (!read_setting(path, text, sizeof(text))) return 0;
    } else if (!read_setting("/etc/keyboards/default", text,
                             sizeof(text))) {
        return 0;
    }
    if (strcmp(text, "us.lyt") == 0) *value = 0;
    else if (strcmp(text, "fr-azerty.lyt") == 0) *value = 1;
    else return 0;
    return 1;
}

static int parse_network_autostart(const char* text, int* value)
{
    size_t offset = 0;
    int enabled = -1;

    while (text[offset]) {
        char line[32];
        size_t length = 0;

        while (text[offset] == '\n' || text[offset] == '\r') offset++;
        while (text[offset] == ' ' || text[offset] == '\t') offset++;
        if (text[offset] == '\n' || text[offset] == '\r') {
            offset++;
            continue;
        }
        if (!text[offset] || text[offset] == '#') {
            while (text[offset] && text[offset] != '\n') offset++;
            continue;
        }
        while (text[offset] && text[offset] != '\n' &&
               text[offset] != '\r') {
            if (length + 1 >= sizeof(line)) return 0;
            line[length++] = text[offset++];
        }
        while (length && (line[length - 1] == ' ' ||
                          line[length - 1] == '\t'))
            length--;
        line[length] = 0;
        if (enabled >= 0) return 0;
        if (strcmp(line, "autostart=1") == 0) enabled = 1;
        else if (strcmp(line, "autostart=0") == 0) enabled = 0;
        else return 0;
    }
    if (enabled < 0) return 0;
    *value = enabled ? 0 : 1;
    return 1;
}

static int load_cursor_type_setting(int* value)
{
    char text[32];
    if (!read_setting("/etc/cursors/type", text, sizeof(text)))
        return 0;
    if (strcmp(text, "bar") == 0 || strcmp(text, "|") == 0 ||
        strcmp(text, "vertical") == 0)
        *value = 0;
    else if (strcmp(text, "underline") == 0 ||
             strcmp(text, "underscore") == 0 ||
             strcmp(text, "_") == 0 ||
             strcmp(text, "line") == 0)
        *value = 1;
    else
        return 0;
    return 1;
}

static int load_setting(int setting, int* value)
{
    char text[128];

    switch (setting) {
    case 0:
        return load_keyboard_setting(value);
    case 1:
        if (!read_setting("/etc/cursors/default", text, sizeof(text)))
            return 0;
        for (int i = 0;
             i < (int)(sizeof(cursor_themes) / sizeof(cursor_themes[0]));
             i++) {
            if (strcmp(text, cursor_themes[i]) == 0) {
                *value = i;
                return 1;
            }
        }
        return 0;
    case 2:
        return load_cursor_type_setting(value);
    case 3:
        if (!read_setting("/etc/network.conf", text, sizeof(text)))
            return 0;
        return parse_network_autostart(text, value);
    case 4:
        if (sys_power_info() != 1 ||
            !read_setting("/etc/power.conf", text, sizeof(text)) ||
            strncmp(text, "idle_minutes=", 13) != 0)
            return 0;
        for (int i = 0; i < 5; i++) {
            char expected[32];
            snprintf(expected, sizeof(expected), "idle_minutes=%u",
                     suspend_minutes[i]);
            if (strcmp(text, expected) == 0) {
                *value = (int)suspend_minutes[i];
                return 1;
            }
        }
        return 0;
    case 5: {
        int count = sys_font_count();
        int font = sys_font_get();
        if (count < 2 || font < 0 || font >= count) return 0;
        *value = font;
        return 1;
    }
    default:
        return 0;
    }
}

static void load_settings(int values[SETTINGS_COUNT],
                          int loaded[SETTINGS_COUNT])
{
    for (int i = 0; i < SETTINGS_COUNT; i++)
        loaded[i] = load_setting(i, &values[i]);
}

static int available_setting_count(const int loaded[SETTINGS_COUNT])
{
    int count = 0;
    for (int i = 0; i < SETTINGS_COUNT; i++)
        if (loaded[i]) count++;
    return count;
}

static int setting_for_selection(const int loaded[SETTINGS_COUNT],
                                int selection)
{
    for (int i = 0; i < SETTINGS_COUNT; i++) {
        if (!loaded[i]) continue;
        if (!selection--) return i;
    }
    return -1;
}

static int personal_layout_setting_path(char* path, size_t capacity)
{
    user_t user;
    int length;
    if (sys_user_current(&user) <= 0) return 0;
    length = snprintf(path, capacity, "%s/.config/keyboards/default",
                     user.home);
    return length >= 0 && (size_t)length < capacity;
}

static int save_value(int setting, int value)
{
    static const char network_on[] = "autostart=1\n";
    static const char network_off[] = "autostart=0\n";
    const char* path;
    const char* data;
    char power_data[32];
    size_t length;
    char personal_path[128];

    switch (setting) {
    case 0:
        if (value < 0 || value > 1) return -1;
        if (!personal_layout_setting_path(personal_path,
                                          sizeof(personal_path)))
            return -1;
        path = personal_path;
        data = value ? "fr-azerty.lyt" : "us.lyt";
        length = strlen(data);
        break;
    case 1:
        if (value < 0 || value >=
                (int)(sizeof(cursor_themes) / sizeof(cursor_themes[0])))
            return -1;
        path = "/etc/cursors/default";
        data = cursor_themes[value];
        length = strlen(data);
        break;
    case 2:
        if (value < 0 || value >=
                (int)(sizeof(cursor_types) / sizeof(cursor_types[0])))
            return -1;
        path = "/etc/cursors/type";
        data = cursor_types[value];
        length = strlen(data);
        break;
    case 3:
        path = "/etc/network.conf";
        data = value ? network_off : network_on;
        length = value ? sizeof(network_off) - 1 : sizeof(network_on) - 1;
        break;
    case 4: {
        int length_written = snprintf(power_data, sizeof(power_data),
                                      "idle_minutes=%d\n", value);
        if (length_written < 0 ||
            (size_t)length_written >= sizeof(power_data))
            return -1;
        path = "/etc/power.conf";
        data = power_data;
        length = (size_t)length_written;
        break;
    }
    case 5:
        if (value < 0 || value >= sys_font_count())
            return -1;
        return sys_font_set(value);
    default:
        return -1;
    }
    return sys_file_replace(path, data, length);
}

static const char* setting_label(int setting)
{
    static const char* const labels[SETTINGS_COUNT] = {
        "Keyboard layout",
        "Cursor theme",
        "Cursor type",
        "Network DHCP autostart",
        "Automatic suspend",
        "Display font"
    };
    return labels[setting];
}

static const char* setting_value(int setting, int value)
{
    switch (setting) {
    case 0: return value ? "French AZERTY" : "US";
    case 1: return cursor_themes[value];
    case 2: return cursor_types[value];
    case 3: return value ? "Disabled" : "Enabled";
    case 4:
        if (!value) return "Disabled";
        return value == 5 ? "5 minutes" : value == 10 ? "10 minutes" :
               value == 15 ? "15 minutes" : "30 minutes";
    case 5:
        return value == 0 ? "Bitmap 8x8" :
               value == 1 ? "Bitmap 5x7" :
               value == 2 ? "Noto Sans" : "Unavailable";
    default: return "";
    }
}

static const char* setting_effect(int setting)
{
    switch (setting) {
    case 0: return "Effective immediately for this account's active console.";
    case 1: return "Effective immediately.";
    case 2: return "Changes the caret style used by the editor and text-entry fields.";
    case 3: return "Controls DHCP autostart on the next system startup.";
    case 4: return "Enters ACPI S3 after input idle; wake with the power button.";
    case 5: return "Changes the console font immediately.";
    default: return "";
    }
}

static void clear_settings_row(const createos_ui_t* ui, uint32_t row,
                               uint32_t from)
{
    static const char spaces[] = "                                ";
    if (!ui || row >= ui->rows || from >= ui->columns) return;
    if (sys_set_cursor((int)from, (int)row) < 0) return;
    while (from < ui->columns) {
        uint32_t count = ui->columns - from;
        char padding[sizeof(spaces)];
        if (count >= sizeof(spaces)) count = sizeof(spaces) - 1;
        memcpy(padding, spaces, count);
        padding[count] = 0;
        if (sys_puts(padding, 0x07) < 0) return;
        from += count;
    }
}

static void draw_settings(const createos_ui_t* ui, int selected,
                          const int values[SETTINGS_COUNT],
                          const int loaded[SETTINGS_COUNT],
                          const char* message)
{
    int visible_selection = 0;
    (void)createos_ui_write_centered(ui, 1, "CREATEOS SETTINGS", 0x0B);
    (void)createos_ui_write_at(
        ui, 0, 2, "  Use Up/Down to select, Left/Right to change, Q to exit.",
        0x08);
    clear_settings_row(ui, 2,
        (uint32_t)strlen("  Use Up/Down to select, Left/Right to change, Q to exit."));
    for (int i = 0; i < SETTINGS_COUNT; i++) {
        int is_selected;
        int row = 4 + i;
        const char* value = loaded[i]
            ? setting_value(i, values[i]) : "Unavailable";
        is_selected = loaded[i] && visible_selection++ == selected;
        (void)sys_set_cursor(0, row);
        sys_puts(is_selected ? "  > " : "    ",
                 is_selected ? 0x0F : 0x07);
        sys_puts(setting_label(i), is_selected ? 0x0F : 0x07);
        sys_puts(": ", is_selected ? 0x0F : 0x07);
        sys_puts(value,
                 loaded[i] ? 0x0A : 0x0C);
        clear_settings_row(ui, (uint32_t)row,
            (uint32_t)(4 + strlen(setting_label(i)) + 2 + strlen(value)));
    }
    const char* status;
    int status_color;
    if (message) {
        status = message;
        status_color = 0x0E;
    } else if (!available_setting_count(loaded)) {
        status = "No settings can be changed; check config and permissions.";
        status_color = 0x0C;
    } else if (available_setting_count(loaded) < SETTINGS_COUNT) {
        status = "Some settings are unavailable; check config and permissions.";
        status_color = 0x0E;
    } else {
        status = "Only settings with implemented effects are shown.";
        status_color = 0x08;
    }
    (void)createos_ui_write_at(ui, 0, 11, status, status_color);
    clear_settings_row(ui, 11, (uint32_t)strlen(status));
    int setting = setting_for_selection(loaded, selected);
    if (setting >= 0) {
        const char* prefix = "  Effect: ";
        (void)sys_set_cursor(0, 12);
        sys_puts(prefix, 0x08);
        sys_puts(setting_effect(setting), 0x08);
        clear_settings_row(ui, 12,
            (uint32_t)(strlen(prefix) + strlen(setting_effect(setting))));
    } else {
        clear_settings_row(ui, 12, 0);
    }
}

int main(int argc, char* argv[])
{
    createos_ui_t ui;
    int values[SETTINGS_COUNT] = {0};
    int loaded[SETTINGS_COUNT] = {0};
    int selected = 0;
    int visible_count;
    int redraw = 1;
    const char* message = 0;
    (void)argc;
    (void)argv;

    load_settings(values, loaded);
    visible_count = available_setting_count(loaded);

    if (createos_ui_init(&ui) < 0)
        return 1;
    (void)createos_ui_clear();
    for (;;) {
        if (redraw) {
            draw_settings(&ui, selected, values, loaded, message);
            redraw = 0;
        }
        int key = createos_ui_read_key();
        if (key == KEY_MOUSE_MOVE || key == KEY_MOUSE_LEFT) {
            int previous_selection = selected;
            const char* previous_message = message;
            int position = createos_ui_mouse_position();
            int mouse_y = ((unsigned int)position >> 16) & 0xFFFF;
            int setting = mouse_y >= 4 && mouse_y < 4 + SETTINGS_COUNT
                ? mouse_y - 4 : -1;
            if (setting >= 0 && loaded[setting]) {
                int selected_index = 0;
                for (int i = 0; i < setting; i++)
                    if (loaded[i]) selected_index++;
                selected = selected_index;
                if (set_pointer_mode(1) < 0)
                    message = "Pointer appearance could not be changed.";
                else if (selected != previous_selection)
                    message = 0;
                if (key == KEY_MOUSE_LEFT)
                    key = KEY_RIGHT;
                else {
                    if (selected != previous_selection ||
                        message != previous_message) {
                        redraw = 1;
                        message = 0;
                    }
                    continue;
                }
            } else if (set_pointer_mode(0) < 0) {
                message = "Pointer appearance could not be changed.";
                redraw = 1;
            } else if (key == KEY_MOUSE_MOVE) {
                continue;
            }
        }
        if (key == 'q' || key == 'Q' ||
            key == KEY_INTERRUPT || key == KEY_CTRL_C) break;
        if (key == KEY_UP && selected > 0) {
            selected--;
            message = 0;
            redraw = 1;
        } else if (key == KEY_DOWN && selected + 1 < visible_count) {
            selected++;
            message = 0;
            redraw = 1;
        }
        else if (visible_count > 0 &&
                 (key == KEY_LEFT || key == KEY_RIGHT || key == '\n' ||
                  key == '\r')) {
            int setting = setting_for_selection(loaded, selected);
            int next = values[setting];
            if (setting == 1) {
                int theme_count =
                    (int)(sizeof(cursor_themes) / sizeof(cursor_themes[0]));
                next = key == KEY_LEFT
                    ? (values[setting] + theme_count - 1) % theme_count
                    : (values[setting] + 1) % theme_count;
            } else if (setting == 2)
                next = key == KEY_LEFT
                    ? (values[setting] + 1) % 2
                    : (values[setting] + 1) % 2;
            else if (setting == 4) {
                int index = 0;
                while (index < 5 &&
                       suspend_minutes[index] != (unsigned int)values[setting])
                    index++;
                if (index >= 5) index = 3;
                index = key == KEY_LEFT ? (index + 4) % 5 : (index + 1) % 5;
                next = (int)suspend_minutes[index];
            }
            else if (setting == 5) {
                int font_count = sys_font_count();
                if (font_count < 2) font_count = 2;
                next = key == KEY_LEFT
                    ? (values[setting] + font_count - 1) % font_count
                    : (values[setting] + 1) % font_count;
            } else
                next = !values[setting];
            int update_failed = 0;
            if (setting == 4 &&
                sys_power_set_idle((unsigned int)next) < 0)
                update_failed = 1;
            if (!update_failed && save_value(setting, next) < 0) {
                if (setting == 4)
                    (void)sys_power_set_idle((unsigned int)values[setting]);
                update_failed = 1;
            }
            if (update_failed) {
                load_settings(values, loaded);
                visible_count = available_setting_count(loaded);
                if (selected >= visible_count)
                    selected = visible_count ? visible_count - 1 : 0;
                message = "Save/apply failed; check permissions and storage.";
            } else {
                values[setting] = next;
                loaded[setting] = 1;
                if (setting == 5 &&
                    createos_ui_refresh_dimensions(&ui) < 0)
                    message = "Font changed, but display dimensions could not be refreshed.";
                if (setting == 1 && pointer_mode == 1) {
                    pointer_mode = -1;
                    if (set_pointer_mode(1) < 0)
                        message = "Pointer appearance could not be changed.";
                }
                if (!message)
                    message = setting == 0
                    ? "Saved and applied to this console."
                    : setting == 1
                        ? "Saved and applied immediately."
                        : setting == 2
                            ? "Saved and applied to the editor caret."
                            : setting == 3
                                ? "Saved; DHCP autostart changes next startup."
                                : setting == 4
                                    ? "Automatic suspend policy applied."
                                    : "Display font changed immediately.";
            }
            redraw = 1;
        }
    }
    (void)set_pointer_mode(0);
    (void)createos_ui_shutdown();
    return 0;
}
