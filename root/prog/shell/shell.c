// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * User-space Shell for CreateOS
 * Built against user_syscall.h, links with userlib + syscalls
 * NO built-in commands - ALL commands are external binaries in /bin/ and /usr/bin/
 * Uses syscalls for all operations
 */
#include "user_syscall.h"
#include "shell_modules.h"

#define MAX_INPUT 256
#define MAX_PATH 256

/* Global state */
static char cwd[MAX_PATH] = "/";
static char tty_name[8] = "tty0";
static char completion_names[64][USER_NAME_MAX];
static char completion_matches[64][MAX_PATH];
static uint8_t completion_colors[64];
static int completion_count;
static int selection_anchor = -1;
static int selection_cursor = -1;
static int selection_active;
static int mouse_selecting;
static int input_start_x;
static int input_start_y;
static int input_visible_start;
static int input_visible_width;
static int input_terminal_width = 80;
static int input_terminal_height = 25;
static int shell_caret_enabled;
static char shell_caret_character = '|';
static int completion_menu_start = -1;
static int completion_menu_rows;

static void load_shell_caret_type(void)
{
    char text[32];
    ssize_t count = sys_read_at("/etc/cursors/type", text,
                                sizeof(text) - 1, 0);
    if (count <= 0 || (size_t)count >= sizeof(text))
        return;
    text[count] = 0;
    while (count > 0 &&
           (text[count - 1] == '\n' || text[count - 1] == '\r'))
        text[--count] = 0;
    if (strcmp(text, "underline") == 0 ||
        strcmp(text, "underscore") == 0 ||
        strcmp(text, "_") == 0 ||
        strcmp(text, "line") == 0)
        shell_caret_character = '_';
    else if (strcmp(text, "bar") == 0 || strcmp(text, "|") == 0 ||
             strcmp(text, "vertical") == 0)
        shell_caret_character = '|';
}

/* Forward declarations */
static void refresh_shell_state(void);
static void read_line(char* buf, int max);
static int autocomplete(char* buf, int* len, int* cursor, int max,
                        int start, int* completion_end);
static void completion_show_menu(int selected);
static void completion_clear_menu(void);
static int completion_apply(char* buf, int* len, int* cursor,
                            int start, int* end, int selected, int max);
static void redraw_line(const char* buf, int len, int cursor, int erase_to);

static int selected_range(int* start, int* end)
{
    if (!selection_active || selection_anchor == selection_cursor)
        return 0;
    *start = selection_anchor < selection_cursor
        ? selection_anchor : selection_cursor;
    *end = selection_anchor > selection_cursor
        ? selection_anchor : selection_cursor;
    return 1;
}

static int line_has_selection(void)
{
    int start, end;
    return selected_range(&start, &end);
}

static int mouse_position_in_line(int length)
{
    int position = sys_tty_mouse_position();
    int x = position & 0xFFFF;
    int y = (int)(((unsigned int)position >> 16) & 0xFFFF);
    if (y != input_start_y) return x < input_start_x ? 0 : length;
    int offset = input_visible_start + x - input_start_x;
    if (offset < 0) return 0;
    if (offset > length) return length;
    return offset;
}

static int delete_line_selection(char* buf, int* length, int* cursor)
{
    int start, end;
    if (!selected_range(&start, &end)) return 0;
    memmove(buf + start, buf + end, (size_t)(*length - end + 1));
    *length -= end - start;
    *cursor = start;
    selection_active = 0;
    selection_anchor = selection_cursor = -1;
    return 1;
}

static int copy_line_selection(const char* buf)
{
    int start, end;
    return selected_range(&start, &end) &&
           sys_clipboard_set(buf + start, (size_t)(end - start)) == 0;
}

static void completion_add(const char* match, uint8_t color)
{
    if (!match || !match[0] || completion_count >= 64 ||
        strlen(match) >= MAX_PATH)
        return;
    for (int i = 0; i < completion_count; i++) {
        if (strcmp(completion_matches[i], match) == 0)
            return;
    }
    strcpy(completion_matches[completion_count], match);
    completion_colors[completion_count] = color;
    completion_count++;
}

static void completion_sort(void)
{
    for (int i = 1; i < completion_count; i++) {
        char match[MAX_PATH];
        uint8_t color = completion_colors[i];
        strcpy(match, completion_matches[i]);
        int j = i;
        while (j > 0 && strcmp(completion_matches[j - 1], match) > 0) {
            strcpy(completion_matches[j], completion_matches[j - 1]);
            completion_colors[j] = completion_colors[j - 1];
            j--;
        }
        strcpy(completion_matches[j], match);
        completion_colors[j] = color;
    }
}

static void completion_common_prefix(char* output, size_t output_size)
{
    size_t length;
    if (!output || output_size == 0 || completion_count == 0) return;
    strncpy(output, completion_matches[0], output_size - 1);
    output[output_size - 1] = 0;
    length = strlen(output);
    for (int i = 1; i < completion_count; i++) {
        size_t j = 0;
        while (j < length && completion_matches[i][j] &&
               output[j] == completion_matches[i][j])
            j++;
        length = j;
        output[length] = 0;
    }
}

static int completion_path_join(char* output, size_t output_size,
                                const char* parent, const char* name)
{
    int written;
    if (!output || !output_size || !parent || !name) return 0;
    written = snprintf(output, output_size, "%s%s%s", parent,
                       strcmp(parent, "/") == 0 ? "" : "/", name);
    return written >= 0 && (size_t)written < output_size;
}

static int completion_collect_command(const char* prefix)
{
    int prefix_length = (int)strlen(prefix);
    int count = sys_list("/bin", completion_names, 64);
    if (count < 0) count = 0;
    if (count > 64) count = 64;
    if (count < 64) {
        int extra = sys_list("/usr/bin", completion_names + count, 64 - count);
        if (extra > 0) count += extra;
    }

    for (int i = 0; i < count && completion_count < 64; i++) {
        char name[USER_NAME_MAX];
        size_t length = strlen(completion_names[i]);
        if (length > 4 && strcmp(completion_names[i] + length - 4, ".bin") == 0)
            length -= 4;
        if (length >= sizeof(name)) length = sizeof(name) - 1;
        memcpy(name, completion_names[i], length);
        name[length] = 0;
        if (strncmp(name, prefix, (size_t)prefix_length) != 0)
            continue;
        completion_add(name, 0x0A);
    }
    return completion_count;
}

static int has_suffix(const char* name, const char* suffix)
{
    size_t name_length = strlen(name);
    size_t suffix_length = strlen(suffix);
    return name_length >= suffix_length &&
           strcmp(name + name_length - suffix_length, suffix) == 0;
}

static int completion_collect_applications(const char* prefix)
{
    const char* directories[] = {"/app", "/app/system", NULL};
    size_t prefix_length = strlen(prefix);

    for (int directory = 0; directories[directory] &&
         completion_count < 64; directory++) {
        int count = sys_list(directories[directory], completion_names, 64);
        if (count < 0) continue;
        if (count > 64) count = 64;
        for (int i = 0; i < count && completion_count < 64; i++) {
            char display_name[USER_NAME_MAX];
            size_t length = strlen(completion_names[i]);
            if (has_suffix(completion_names[i], ".APPL")) {
                if (length >= sizeof(display_name)) continue;
                memcpy(display_name, completion_names[i], length + 1);
            } else if (has_suffix(completion_names[i], ".link")) {
                length -= 5;
                if (length + 6 > sizeof(display_name)) continue;
                memcpy(display_name, completion_names[i], length);
                memcpy(display_name + length, ".APPL", 6);
            } else {
                continue;
            }
            if (strncmp(display_name, prefix, prefix_length) == 0)
                completion_add(display_name, 0x0A);
        }
    }
    return completion_count;
}

static int completion_is_open_application(const char* buf, int start)
{
    int command_start = 0;
    int command_end;
    while (buf[command_start] == ' ' || buf[command_start] == '\t')
        command_start++;
    command_end = command_start;
    while (buf[command_end] && buf[command_end] != ' ' &&
           buf[command_end] != '\t')
        command_end++;
    if (command_end - command_start != 4 ||
        strncmp(buf + command_start, "open", 4) != 0)
        return 0;
    while (buf[command_end] == ' ' || buf[command_end] == '\t')
        command_end++;
    return command_end == start;
}

static int completion_collect_path(const char* prefix)
{
    char directory[MAX_PATH];
    char absolute_directory[MAX_PATH];
    char absolute_child[MAX_PATH];
    char token_directory[MAX_PATH];
    char child[MAX_PATH];
    const char* slash = NULL;
    const char* basename = slash ? slash + 1 : prefix;
    size_t prefix_length = strlen(prefix);
    size_t directory_length;
    int listed;

    for (const char* p = prefix; *p; p++)
        if (*p == '/') slash = p;
    basename = slash ? slash + 1 : prefix;
    directory_length = slash ? (size_t)(slash - prefix) : 0;
    token_directory[0] = 0;
    if (slash) {
        if (directory_length + 2 > sizeof(token_directory)) return 0;
        memcpy(token_directory, prefix, directory_length + 1);
        token_directory[directory_length + 1] = 0;
        if (prefix[0] == '/') {
            if (directory_length == 0) {
                strcpy(directory, "/");
            } else {
                memcpy(directory, prefix, directory_length);
                directory[directory_length] = 0;
            }
        } else if (directory_length == 0) {
            strcpy(directory, cwd);
        } else {
            char relative_directory[MAX_PATH];
            if (directory_length >= sizeof(relative_directory)) return 0;
            memcpy(relative_directory, prefix, directory_length);
            relative_directory[directory_length] = 0;
            if (completion_path_join(directory, sizeof(directory), cwd,
                                     relative_directory) == 0)
                return 0;
        }
    } else {
        strcpy(directory, cwd);
    }

    if (directory[0] == '/') {
        if (strlen(directory) >= sizeof(absolute_directory)) return 0;
        strcpy(absolute_directory, directory);
    } else if (!completion_path_join(absolute_directory,
                                     sizeof(absolute_directory), cwd,
                                     directory)) {
        return 0;
    }

    listed = sys_list(absolute_directory, completion_names, 64);
    if (listed <= 0) return 0;
    if (listed > 64) listed = 64;
    for (int i = 0; i < listed && completion_count < 64; i++) {
        fs_stat_t status;
        size_t name_length = strlen(completion_names[i]);
        size_t token_directory_length = strlen(token_directory);
        int written;
        uint8_t color = 0x0F;
        if ((completion_names[i][0] == '.' && basename[0] != '.') ||
            strncmp(completion_names[i], basename, prefix_length -
                    (size_t)(basename - prefix)) != 0)
            continue;
        if (token_directory_length + name_length + 2 > sizeof(child))
            continue;
        memcpy(child, token_directory, token_directory_length);
        memcpy(child + token_directory_length, completion_names[i],
               name_length + 1);
        if (!completion_path_join(absolute_child, sizeof(absolute_child),
                                  absolute_directory, completion_names[i]))
            continue;
        if (sys_stat(absolute_child, &status) == 0) {
            if (status.type == CFS5_INODE_DIR) {
                color = 0x0B;
                child[token_directory_length + name_length] = '/';
                child[token_directory_length + name_length + 1] = 0;
            } else if (status.mode & 0111) {
                color = 0x0A;
            }
        }
        written = (int)strlen(child);
        if (written < (int)sizeof(child))
            completion_add(child, color);
    }
    return completion_count;
}

/* Read line with editing and history */
static void read_line(char* buf, int max) {
    int len = 0;
    int cursor = 0;
    int completion_active = 0;
    int completion_start = 0;
    int completion_end = 0;
    int completion_selected = -1;
    int clipboard_length;
    char clipboard[MAX_INPUT];
    if (!buf || max < 2) return;
    buf[0] = 0;
    tty_info_t tty_info;
    input_terminal_width = 80;
    input_terminal_height = 25;
    for (int i = 0; i < 5; i++) {
        if (sys_tty_info(i, &tty_info) == 0 &&
            strcmp(tty_info.name, tty_name) == 0 &&
            tty_info.width > 0) {
            input_terminal_width = (int)tty_info.width;
            if (tty_info.height > 0)
                input_terminal_height = (int)tty_info.height;
            break;
        }
    }
    shell_history_reset();
    selection_anchor = selection_cursor = -1;
    selection_active = 0;
    mouse_selecting = 0;
    int input_position = sys_tty_cursor_position();
    if (input_position >= 0) {
        input_start_x = input_position & 0xFFFF;
        input_start_y = (int)(((unsigned int)input_position >> 16) & 0xFFFF);
    } else {
        input_start_x = input_start_y = 0;
    }
    (void)sys_tty_shell_input(1);
    shell_caret_enabled = 1;
    redraw_line(buf, len, cursor, len);
    unsigned long drawn_caret_phase = sys_ticks() / 25u;

    while (1) {
        unsigned long caret_phase = sys_ticks() / 25u;
        if (caret_phase != drawn_caret_phase) {
            drawn_caret_phase = caret_phase;
            redraw_line(buf, len, cursor, len);
        }
        int input = sys_tty_pollchar();
        if (!input) {
            (void)sys_sleep(10);
            continue;
        }
        char c = (char)input;
        unsigned char key = (unsigned char)c;
        if (completion_active &&
            key != KEY_UP && key != KEY_DOWN &&
            c != '\t' && key != KEY_TAB && key != KEY_SHIFT_TAB) {
            completion_clear_menu();
            completion_active = 0;
        }
        if (c == '\n' || c == '\r') {
            completion_active = 0;
            selection_active = 0;
            mouse_selecting = 0;
            shell_caret_enabled = 0;
            redraw_line(buf, len, cursor, len);
            (void)sys_tty_shell_input(0);
            sys_putchar('\n');
            buf[len] = 0;
            return;
        }

        if (key == KEY_INTERRUPT ||
            (key == KEY_CTRL_C && !line_has_selection())) {
            completion_active = 0;
            selection_active = 0;
            mouse_selecting = 0;
            shell_caret_enabled = 0;
            redraw_line(buf, len, cursor, len);
            (void)sys_tty_shell_input(0);
            sys_puts("^C\n", 0x0C);
            buf[0] = 0;
            shell_history_reset();
            return;
        }

        if (key == KEY_CTRL_C) {
            if (copy_line_selection(buf))
                selection_active = 0;
            else
                sys_puts("\a", 0x0E);
            redraw_line(buf, len, cursor, len);
            continue;
        }

        if (key == KEY_CTRL_X) {
            if (copy_line_selection(buf)) {
                int old_len = len;
                (void)delete_line_selection(buf, &len, &cursor);
                redraw_line(buf, len, cursor, old_len);
            } else {
                sys_puts("\a", 0x0E);
            }
            continue;
        }

        if (key == KEY_CTRL_V) {
            clipboard_length = sys_clipboard_get(clipboard, sizeof(clipboard));
            if (clipboard_length < 0 || clipboard_length >= max) {
                sys_puts("\a", 0x0E);
                continue;
            }
            int selection_start = 0;
            int selection_end = 0;
            int selection_length = selected_range(&selection_start,
                                                   &selection_end)
                ? selection_end - selection_start : 0;
            if (len - selection_length + clipboard_length >= max) {
                sys_puts("\a", 0x0E);
                continue;
            }
            int old_len = len;
            (void)delete_line_selection(buf, &len, &cursor);
            memmove(buf + cursor + clipboard_length, buf + cursor,
                    (size_t)(len - cursor + 1));
            memcpy(buf + cursor, clipboard, (size_t)clipboard_length);
            len += clipboard_length;
            cursor += clipboard_length;
            redraw_line(buf, len, cursor, old_len);
            continue;
        }

        if (key == KEY_MOUSE_LEFT) {
            int position = mouse_position_in_line(len);
            selection_anchor = selection_cursor = position;
            cursor = position;
            selection_active = 1;
            mouse_selecting = 1;
            redraw_line(buf, len, cursor, len);
            continue;
        }
        if (key == KEY_MOUSE_DRAG && mouse_selecting) {
            selection_cursor = mouse_position_in_line(len);
            cursor = selection_cursor;
            redraw_line(buf, len, cursor, len);
            continue;
        }
        if (key == KEY_MOUSE_RELEASE) {
            if (mouse_selecting) {
                selection_cursor = mouse_position_in_line(len);
                cursor = selection_cursor;
                redraw_line(buf, len, cursor, len);
            }
            mouse_selecting = 0;
            continue;
        }

        if (c == '\b' || c == 127) {
            completion_active = 0;
            int old_len = len;
            if (delete_line_selection(buf, &len, &cursor)) {
                redraw_line(buf, len, cursor, old_len);
            } else if (cursor > 0) {
                int old_len = len;
                for (int i = cursor - 1; i < len - 1; i++) {
                    buf[i] = buf[i + 1];
                }
                len--;
                cursor--;
                buf[len] = 0;
                if (cursor == len) {
                    sys_putchar('\b');
                    sys_putchar(' ');
                    sys_putchar('\b');
                } else {
                    redraw_line(buf, len, cursor, old_len);
                }
            }
            continue;
        }

        if (key == KEY_DELETE) {
            completion_active = 0;
            int old_len = len;
            if (delete_line_selection(buf, &len, &cursor)) {
                redraw_line(buf, len, cursor, old_len);
            } else if (cursor < len) {
                int old_len = len;
                for (int i = cursor; i < len - 1; i++)
                    buf[i] = buf[i + 1];
                len--;
                buf[len] = 0;
                redraw_line(buf, len, cursor, old_len);
            }
            continue;
        }

        if (key == KEY_LEFT) {
            completion_active = 0;
            selection_active = 0;
            if (cursor > 0) {
                cursor--;
                redraw_line(buf, len, cursor, len);
            }
            continue;
        }

        if (key == KEY_RIGHT) {
            completion_active = 0;
            selection_active = 0;
            if (cursor < len) {
                cursor++;
                redraw_line(buf, len, cursor, len);
            }
            continue;
        }

        if (key == KEY_SHIFT_LEFT || key == KEY_SHIFT_RIGHT) {
            if (!selection_active) {
                selection_anchor = cursor;
                selection_active = 1;
            }
            if (key == KEY_SHIFT_LEFT && cursor > 0) cursor--;
            if (key == KEY_SHIFT_RIGHT && cursor < len) cursor++;
            selection_cursor = cursor;
            redraw_line(buf, len, cursor, len);
            continue;
        }

        if (key == KEY_UP) {
            if (completion_active) {
                completion_selected = completion_selected < 0 ?
                    completion_count - 1 :
                    (completion_selected + completion_count - 1) %
                    completion_count;
                int old_len = len;
                completion_apply(buf, &len, &cursor, completion_start,
                                 &completion_end, completion_selected, max);
                redraw_line(buf, len, cursor, old_len);
                completion_show_menu(completion_selected);
                continue;
            }
            if (shell_history_previous(buf, (size_t)max)) {
                int old_len = len;
                len = (int)strlen(buf);
                cursor = len;
                redraw_line(buf, len, cursor, old_len);
            }
            continue;
        }

        if (key == KEY_DOWN) {
            if (completion_active) {
                completion_selected = (completion_selected + 1) %
                                      completion_count;
                int old_len = len;
                completion_apply(buf, &len, &cursor, completion_start,
                                 &completion_end, completion_selected, max);
                redraw_line(buf, len, cursor, old_len);
                completion_show_menu(completion_selected);
                continue;
            }
            int old_len = len;
            if (shell_history_next(buf, (size_t)max)) {
                len = (int)strlen(buf);
                cursor = len;
            }
            redraw_line(buf, len, cursor, old_len);
            continue;
        }

        if (c == '\t' || key == KEY_TAB || key == KEY_SHIFT_TAB) {
            int old_len = len;
            if (completion_active) {
                completion_selected = key == KEY_SHIFT_TAB ?
                    (completion_selected < 0 ? completion_count - 1 :
                     (completion_selected + completion_count - 1) %
                         completion_count) :
                    (completion_selected + 1) % completion_count;
                completion_apply(buf, &len, &cursor, completion_start,
                                 &completion_end, completion_selected, max);
                redraw_line(buf, len, cursor, old_len);
                completion_show_menu(completion_selected);
            } else if (c == '\t' || key == KEY_TAB) {
                int start = cursor;
                while (start > 0 && buf[start - 1] != ' ' &&
                       buf[start - 1] != '\t')
                    start--;
                completion_count = autocomplete(buf, &len, &cursor, max,
                                                start, &completion_end);
                if (completion_count > 1) {
                    completion_start = start;
                    completion_selected = -1;
                    completion_active = 1;
                    completion_show_menu(-1);
                    redraw_line(buf, len, cursor, old_len);
                } else if (len != old_len) {
                    redraw_line(buf, len, cursor, old_len);
                }
            } else if (cursor > 0) {
                cursor--;
                redraw_line(buf, len, cursor, len);
            }
            continue;
        }

        if (key < 0x20 || key >= 0x80) continue;

        if (len < max - 1 || selection_active) {
            completion_active = 0;
            int old_len = len;
            (void)delete_line_selection(buf, &len, &cursor);
            selection_active = 0;
            if (len >= max - 1)
                continue;
            int append = cursor == len;
            for (int i = len; i > cursor; i--) {
                buf[i] = buf[i - 1];
            }
            buf[cursor] = c;
            len++;
            cursor++;
            buf[len] = 0;
            if (append)
                sys_putchar(c);
            else
                redraw_line(buf, len, cursor, old_len);
        }
    }
}

static void redraw_line(const char* buf, int len, int cursor, int erase_to) {
    int selection_start = 0;
    int selection_end = 0;
    int has_selection = selected_range(&selection_start, &selection_end);
    if (input_terminal_width <= input_start_x ||
        input_start_y < 0 || cursor < 0 || cursor > len)
        return;

    input_visible_width = input_terminal_width - input_start_x;
    input_visible_start = cursor >= input_visible_width
        ? cursor - input_visible_width + 1 : 0;
    if (input_visible_start > len)
        input_visible_start = len;
    if (erase_to > len && input_visible_start == 0 &&
        erase_to < input_visible_width)
        len = erase_to;

    int redraw_width = input_visible_width;
    if (input_visible_start == 0) {
        redraw_width = len < input_visible_width ? len : input_visible_width;
        if (redraw_width <= 0 && cursor == 0)
            redraw_width = 1;
    }
    if (sys_set_cursor(input_start_x, input_start_y) < 0)
        return;
    for (int i = 0; i < redraw_width; i++)
        sys_puts(" ", 0x0F);
    if (sys_set_cursor(input_start_x, input_start_y) < 0)
        return;
    int draw_end = len < input_visible_start + input_visible_width
        ? len : input_visible_start + input_visible_width;
    for (int i = input_visible_start; i < draw_end; i++) {
        sys_puts((char[]){buf[i], 0},
                 has_selection && i >= selection_start && i < selection_end
                     ? 0x1F : 0x0F);
    }

    int caret_column = input_start_x + cursor - input_visible_start;
    if (shell_caret_enabled &&
        (sys_ticks() / 25u) % 2u == 0 &&
        caret_column < input_terminal_width) {
        if (sys_set_cursor(caret_column, input_start_y) == 0) {
            sys_puts((char[]){shell_caret_character, 0}, 0x0F);
        }
    }
    (void)sys_set_cursor(caret_column, input_start_y);
}

static int completion_apply(char* buf, int* len, int* cursor,
                            int start, int* end, int selected, int max)
{
    size_t match_length;
    int replaced_length;
    int new_length;
    if (!buf || !len || !cursor || !end || selected < 0 ||
        selected >= completion_count || start < 0 || start > *end ||
        *end > *len)
        return 0;
    match_length = strlen(completion_matches[selected]);
    replaced_length = *end - start;
    new_length = *len - replaced_length + (int)match_length;
    if (new_length >= max) return 0;
    memmove(buf + start + match_length, buf + *end, *len - *end + 1);
    memcpy(buf + start, completion_matches[selected], match_length);
    *len = new_length;
    *cursor = start + (int)match_length;
    *end = *cursor;
    return 1;
}

static int autocomplete(char* buf, int* len, int* cursor, int max,
                        int start, int* completion_end)
{
    char prefix[MAX_PATH];
    char common[MAX_PATH];
    int token_end;
    int prefix_length;
    int suffix_length;
    if (!buf || !len || !cursor || !completion_end || *len < 0 ||
        *len >= max || *cursor < 0 || *cursor > *len ||
        start < 0 || start > *cursor)
        return 0;

    token_end = *cursor;
    while (token_end < *len && buf[token_end] != ' ' &&
           buf[token_end] != '\t')
        token_end++;
    prefix_length = *cursor - start;
    if (prefix_length >= (int)sizeof(prefix)) return 0;
    memcpy(prefix, buf + start, (size_t)prefix_length);
    prefix[prefix_length] = 0;

    completion_count = 0;
    if (start == 0 && !strchr(prefix, '/'))
        completion_collect_command(prefix);
    else if (completion_is_open_application(buf, start))
        completion_collect_applications(prefix);
    else
        completion_collect_path(prefix);
    if (!completion_count) return 0;
    completion_sort();

    *completion_end = token_end;
    if (completion_count == 1) {
        return completion_apply(buf, len, cursor, start, completion_end, 0,
                                max) ? 1 : 0;
    }

    completion_common_prefix(common, sizeof(common));
    size_t common_length = strlen(common);
    if (common_length > (size_t)prefix_length) {
        int extra = (int)(common_length - (size_t)prefix_length);
        suffix_length = *len - *cursor;
        if (*len + extra < max) {
            memmove(buf + *cursor + extra, buf + *cursor,
                    (size_t)suffix_length + 1);
            memcpy(buf + *cursor, common + prefix_length, (size_t)extra);
            *len += extra;
            *cursor += extra;
            *completion_end += extra;
        }
    }
    return completion_count;
}

static void completion_show_menu(int selected)
{
    int width = 1;
    int columns;
    int rows;
    int available_rows;
    int visible_rows;
    int first;
    completion_clear_menu();
    for (int i = 0; i < completion_count; i++) {
        int length = strlen(completion_matches[i]);
        if (length > width) width = length;
    }
    if (input_terminal_width < 1 || input_terminal_height < 1 ||
        completion_count < 1)
        return;
    if (width + 2 > input_terminal_width)
        width = input_terminal_width > 2 ? input_terminal_width - 2 : 1;
    columns = input_terminal_width / (width + 2);
    if (columns < 1) columns = 1;
    rows = (completion_count + columns - 1) / columns;
    available_rows = input_terminal_height - input_start_y - 1;
    if (available_rows > 8) available_rows = 8;
    if (available_rows < 1) return;
    visible_rows = rows < available_rows ? rows : available_rows;
    first = selected >= 0
        ? (selected / (columns * visible_rows)) * (columns * visible_rows)
        : 0;
    completion_menu_rows = visible_rows;
    completion_menu_start = input_start_y + 1;
    for (int row = 0; row < visible_rows; row++) {
        if (sys_set_cursor(0, completion_menu_start + row) < 0) break;
        int last_column = -1;
        for (int column = 0; column < columns; column++) {
            int index = first + row * columns + column;
            if (index >= completion_count) break;
            int color = index == selected ? 0x0E : completion_colors[index];
            size_t length = strlen(completion_matches[index]);
            size_t display_length = length < (size_t)width
                ? length : (size_t)width;
            char display[MAX_PATH];
            memcpy(display, completion_matches[index], display_length);
            display[display_length] = 0;
            sys_puts(display, color);
            last_column = column;
            if (column + 1 < columns && index + 1 < completion_count) {
                for (size_t space = display_length;
                     space < (size_t)width + 2; space++)
                    sys_putchar(' ');
            }
        }
        int used = last_column < 0 ? 0 :
                   (last_column + 1) * (width + 2);
        if (used < input_terminal_width) {
            static const char spaces[] =
                "                                ";
            while (used < input_terminal_width) {
                int count = input_terminal_width - used;
                char padding[sizeof(spaces)];
                if (count >= (int)sizeof(spaces)) count = sizeof(spaces) - 1;
                memcpy(padding, spaces, (size_t)count);
                padding[count] = 0;
                sys_puts(padding, 0x07);
                used += count;
            }
        }
    }
    if (sys_set_cursor(input_start_x, input_start_y) < 0)
        return;
}

static void completion_clear_menu(void)
{
    static const char spaces[] = "                                ";
    if (completion_menu_start < 0 || completion_menu_rows <= 0)
        return;
    for (int row = 0; row < completion_menu_rows; row++) {
        if (sys_set_cursor(0, completion_menu_start + row) < 0)
            break;
        for (int column = 0; column < input_terminal_width;) {
            int count = input_terminal_width - column;
            char padding[sizeof(spaces)];
            if (count >= (int)sizeof(spaces)) count = sizeof(spaces) - 1;
            memcpy(padding, spaces, (size_t)count);
            padding[count] = 0;
            if (sys_puts(padding, 0x07) < 0)
                break;
            column += count;
        }
    }
    completion_menu_start = -1;
    completion_menu_rows = 0;
    (void)sys_set_cursor(input_start_x, input_start_y);
}

static void refresh_shell_state(void) {
    char path[MAX_PATH];
    if (sys_getcwd(path, sizeof(path)) == 0) {
        strncpy(cwd, path, sizeof(cwd) - 1);
        cwd[sizeof(cwd) - 1] = 0;
    }
}

/* Main shell loop */
void shell_run(void) {
    sys_tty_name(tty_name);
    load_shell_caret_type();
    refresh_shell_state();
    shell_history_load();

    while (1) {
        shell_prompt(tty_name, cwd);
        char line[MAX_INPUT];
        read_line(line, sizeof(line));

        if (!line[0]) continue;

        shell_history_add(line);

        shell_execute_line(line);
    }
}

/* Entry point */
void _start(void) {
    shell_run();
    sys_exit(0);
}