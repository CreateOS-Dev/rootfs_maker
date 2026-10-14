#include "user_syscall.h"
#include "createos/uikit.h"

#define EXPLORER_MAX_ITEMS 128
#define EXPLORER_PATH_MAX 256
#define EXPLORER_MAX_ROWS 100
#define EXPLORER_MAX_COLUMNS 160
#define EXPLORER_MAX_VISIBLE_ITEMS (EXPLORER_MAX_ROWS - 9)

typedef struct {
    char path[EXPLORER_PATH_MAX];
    char names[EXPLORER_MAX_ITEMS][USER_NAME_MAX];
    int count;
    int selected;
    int first_visible;
} explorer_state_t;

static char screen_lines[EXPLORER_MAX_ROWS][EXPLORER_MAX_COLUMNS + 1];
static char previous_screen[EXPLORER_MAX_ROWS][EXPLORER_MAX_COLUMNS + 1];
static uint8_t screen_colors[EXPLORER_MAX_ROWS];
static uint8_t previous_colors[EXPLORER_MAX_ROWS];
static int screen_initialized;
static const char* explorer_message;
static int mouse_x = -1;
static int mouse_y = -1;
static int toolbar_hover = -1;
static int pointer_mode = -1;
static int pointer_supported;
static createos_ui_t explorer_ui;
static int mouse_pressed;
static int mouse_dragging;
static char drag_source[EXPLORER_PATH_MAX];
static int drop_target_hover;
static size_t explorer_screen_rows = 25;
static size_t explorer_screen_columns = 79;
static size_t explorer_visible_items = 16;

static void keep_selection_visible(explorer_state_t* state);
static int toolbar_action_at(int column);
static int handle_mouse_click(explorer_state_t* state);
static int has_appl_suffix(const char* path);

static void set_pointer_mode(int mode)
{
    if (pointer_mode == mode)
        return;
    if (sys_tty_pointer_mode(mode) == 0)
        pointer_mode = mode;
    else
        explorer_message = "Pointer appearance could not be changed.";
}

static void explorer_detect_screen_size(void)
{
    tty_info_t info;
    char tty_name[16] = { 0 };
    if (sys_tty_name(tty_name) == 0 && strncmp(tty_name, "ttyS", 4) == 0)
        return;
    if (sys_tty_info(4, &info) == 0 &&
        (info.flags & TTY_FLAG_GRAPHICAL) && info.width >= 80 &&
        info.height >= 25) {
        pointer_supported = 1;
        explorer_screen_columns = info.width;
        explorer_screen_rows = info.height;
    }
    if (explorer_screen_columns >= EXPLORER_MAX_COLUMNS)
        explorer_screen_columns = EXPLORER_MAX_COLUMNS;
    if (explorer_screen_rows >= EXPLORER_MAX_ROWS)
        explorer_screen_rows = EXPLORER_MAX_ROWS;
    explorer_screen_columns--;
    explorer_visible_items = explorer_screen_rows > 9
        ? explorer_screen_rows - 9 : 1;
    if (explorer_visible_items > EXPLORER_MAX_VISIBLE_ITEMS)
        explorer_visible_items = EXPLORER_MAX_VISIBLE_ITEMS;
}

static void format_screen_line(char* output, const char* text)
{
    size_t length = strlen(text);
    if (length > explorer_screen_columns)
        length = explorer_screen_columns;
    memset(output, ' ', explorer_screen_columns);
    memcpy(output, text, length);
    output[explorer_screen_columns] = 0;
}

static int path_join(char* output, size_t capacity, const char* directory,
                     const char* name)
{
    size_t dir_length;
    size_t name_length;
    if (!output || !directory || !name || directory[0] != '/' ||
        !name[0] || strchr(name, '/') || strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0)
        return -1;
    dir_length = strlen(directory);
    name_length = strlen(name);
    if (dir_length + (dir_length > 1) + name_length >= capacity)
        return -1;
    memcpy(output, directory, dir_length);
    if (dir_length > 1) output[dir_length++] = '/';
    memcpy(output + dir_length, name, name_length + 1);
    return 0;
}

static int user_applications_directory(char* directory, size_t capacity,
                                       int create)
{
    user_t user;
    char local_directory[EXPLORER_PATH_MAX];
    fs_stat_t status;
    int length;

    if (sys_user_current(&user) <= 0 || !user.home[0] ||
        user.home[0] != '/')
        return -1;
    length = snprintf(local_directory, sizeof(local_directory),
                      "%s/.local", user.home);
    if (length < 0 || (size_t)length >= sizeof(local_directory))
        return -1;
    if (sys_stat(local_directory, &status) < 0) {
        if (!create || sys_mkdir(local_directory, 0755) < 0)
            return -1;
    } else if (status.type != FS_TYPE_DIR) {
        return -1;
    }

    length = snprintf(directory, capacity, "%s/apps", local_directory);
    if (length < 0 || (size_t)length >= capacity)
        return -1;
    if (sys_stat(directory, &status) < 0) {
        if (!create || sys_mkdir(directory, 0755) < 0)
            return -1;
    } else if (status.type != FS_TYPE_DIR) {
        return -1;
    }
    return 0;
}

static int is_user_application(const char* path)
{
    char directory[EXPLORER_PATH_MAX];
    size_t length;
    if (!has_appl_suffix(path) ||
        user_applications_directory(directory, sizeof(directory), 0) < 0)
        return 0;
    length = strlen(directory);
    return strncmp(path, directory, length) == 0 &&
           path[length] == '/' && path[length + 1] &&
           strchr(path + length + 1, '/') == 0;
}

static int load_directory(explorer_state_t* state, const char* path)
{
    fs_stat_t status;
    explorer_state_t next;
    if (!state || !path || strlen(path) >= sizeof(next.path) ||
        sys_stat(path, &status) < 0 || status.type != FS_TYPE_DIR)
        return -1;
    memset(&next, 0, sizeof(next));
    strcpy(next.path, path);
    next.count = sys_list(path, next.names, EXPLORER_MAX_ITEMS);
    if (next.count < 0 || next.count > EXPLORER_MAX_ITEMS) return -1;

    for (int i = 1; i < next.count; i++) {
        char current[USER_NAME_MAX];
        int j = i;
        strcpy(current, next.names[i]);
        while (j > 0 && strcmp(next.names[j - 1], current) > 0) {
            strcpy(next.names[j], next.names[j - 1]);
            j--;
        }
        strcpy(next.names[j], current);
    }
    *state = next;
    return 0;
}

static int has_appl_suffix(const char* path)
{
    size_t length = strlen(path);
    if (length < 5) return 0;
    return strcmp(path + length - 5, ".APPL") == 0 ||
           strcmp(path + length - 5, ".appl") == 0;
}

static int is_installed_application(const char* path)
{
    static const char system_directory[] = "/app/system/bin/";
    static const char custom_directory[] = "/app/custom/";
    const char* name;
    if (!has_appl_suffix(path))
        return 0;
    if (strncmp(path, system_directory, sizeof(system_directory) - 1) == 0)
        name = path + sizeof(system_directory) - 1;
    else if (strncmp(path, custom_directory, sizeof(custom_directory) - 1) == 0)
        name = path + sizeof(custom_directory) - 1;
    else
        return is_user_application(path);
    return name[0] && strchr(name, '/') == 0;
}

static int copy_application_to_user(const char* source)
{
    uint8_t buffer[4096];
    uint8_t header[20];
    fs_stat_t source_status;
    char destination_directory[EXPLORER_PATH_MAX];
    char destination[EXPLORER_PATH_MAX];
    const char* name = source;
    int source_fd = -1;
    int destination_fd = -1;
    int destination_created = 0;
    int failed = 0;
    uint32_t copied = 0;

    if (!has_appl_suffix(source) ||
        sys_stat(source, &source_status) < 0 ||
        source_status.type != FS_TYPE_FILE ||
        source_status.size < sizeof(header))
        return -1;
    if (user_applications_directory(destination_directory,
                                    sizeof(destination_directory), 1) < 0)
        return -1;
    for (const char* cursor = source; *cursor; cursor++)
        if (*cursor == '/') name = cursor + 1;
    if (!name[0] || strlen(name) >= USER_NAME_MAX)
        return -1;
    {
        int length = snprintf(destination, sizeof(destination), "%s/%s",
                              destination_directory, name);
        if (length < 0 || (size_t)length >= sizeof(destination))
            return -1;
    }

    source_fd = sys_open(source, O_RDONLY, 0);
    if (source_fd < 0)
        return -1;
    if (sys_read(source_fd, header, sizeof(header)) != sizeof(header) ||
        memcmp(header, "\x7F" "ELF", 4) != 0 ||
        header[4] != 2 || header[5] != 1 ||
        header[18] != 0x3E || header[19] != 0) {
        failed = 1;
        goto cleanup;
    }
    if (sys_seek(source_fd, 0, 0) < 0) {
        failed = 1;
        goto cleanup;
    }
    destination_fd = sys_open(destination, O_WRONLY | O_CREAT | O_EXCL,
                              0755);
    if (destination_fd < 0) {
        failed = 1;
        goto cleanup;
    }
    destination_created = 1;

    while (copied < source_status.size) {
        size_t wanted = source_status.size - copied;
        ssize_t count;
        size_t written = 0;
        if (wanted > sizeof(buffer)) wanted = sizeof(buffer);
        count = sys_read(source_fd, buffer, wanted);
        if (count <= 0 || (size_t)count > wanted) {
            failed = 1;
            break;
        }
        while (written < (size_t)count) {
            ssize_t result = sys_write(destination_fd, buffer + written,
                                       (size_t)count - written);
            if (result <= 0 || (size_t)result > (size_t)count - written) {
                failed = 1;
                break;
            }
            written += (size_t)result;
        }
        if (failed) break;
        copied += (uint32_t)count;
    }

cleanup:
    if (source_fd >= 0 && sys_close(source_fd) < 0)
        failed = 1;
    if (destination_fd >= 0 && sys_close(destination_fd) < 0)
        failed = 1;
    if (failed && destination_created && sys_unlink(destination) < 0)
        return -2;
    return failed ? -1 : 0;
}

static void begin_mouse_action(explorer_state_t* state)
{
    int position = createos_ui_mouse_position();
    int y = (int)(((unsigned int)position >> 16) & 0xFFFF);

    mouse_pressed = 1;
    mouse_dragging = 0;
    drag_source[0] = 0;
    drop_target_hover = 0;
    if (y < 4 || y >= 4 + (int)explorer_visible_items)
        return;
    int selected = state->first_visible + y - 4;
    if (selected < 0 || selected >= state->count)
        return;
    state->selected = selected;
    if (!has_appl_suffix(state->names[selected]))
        return;
    if (path_join(drag_source, sizeof(drag_source), state->path,
                  state->names[selected]) < 0)
        drag_source[0] = 0;
}

static void finish_mouse_action(explorer_state_t* state)
{
    int position = createos_ui_mouse_position();
    int x = position & 0xFFFF;
    int y = (int)(((unsigned int)position >> 16) & 0xFFFF);

    if (mouse_dragging) {
        if (y == 3 && drag_source[0]) {
            int result = copy_application_to_user(drag_source);
            if (result == 0) {
                explorer_message =
                    "Installed in ~/.local/apps; press A to open.";
            } else if (result == -2) {
                explorer_message =
                    "Copy failed; partial app could not be removed.";
            } else {
                explorer_message =
                    "Drop failed: use a valid .APPL file and free app name.";
            }
        } else {
            explorer_message = "Drop cancelled; release on the install bar.";
        }
        screen_initialized = 0;
    } else {
        int action = y == 2 ? toolbar_action_at(x) : -1;
        if (action >= 0 || (y >= 4 &&
            y < 4 + (int)explorer_visible_items)) {
            int result = handle_mouse_click(state);
            explorer_message = result < 0 ? "Action failed." : 0;
            screen_initialized = 0;
            if (load_directory(state, state->path) < 0)
                explorer_message = "Directory refresh failed.";
        }
    }
    mouse_pressed = 0;
    mouse_dragging = 0;
    drag_source[0] = 0;
    drop_target_hover = 0;
}

static int resolve_user_application_link(const char* path, char* target,
                                         size_t capacity)
{
    fs_stat_t status;
    char contents[EXPLORER_PATH_MAX];
    size_t length;
    if (strncmp(path, "/home/", 6) != 0 ||
        sys_stat(path, &status) < 0 || status.type != FS_TYPE_FILE ||
        !status.size || status.size >= sizeof(contents) ||
        sys_read_at(path, contents, status.size, 0) != (ssize_t)status.size)
        return 0;
    length = status.size;
    while (length && (contents[length - 1] == '\n' ||
                      contents[length - 1] == '\r'))
        length--;
    if (length < 6 || length >= capacity ||
        memcmp(contents, "/app/", 5) != 0)
        return 0;
    contents[length] = 0;
    if (!is_installed_application(contents)) return 0;
    for (size_t i = 0; i < length; i++)
        if ((unsigned char)contents[i] < 0x20) return 0;
    if (sys_stat(contents, &status) < 0 || status.type != FS_TYPE_FILE)
        return 0;
    memcpy(target, contents, length + 1);
    return 1;
}

static int has_source_suffix(const char* path)
{
    size_t length = strlen(path);
    if (length < 2) return 0;
    return path[length - 2] == '.' &&
           (path[length - 1] == 'c' || path[length - 1] == 'C' ||
            path[length - 1] == 'h' || path[length - 1] == 'H');
}

static int launch_path(const char* executable, const char* argument)
{
    char* arguments[] = {(char*)executable, (char*)argument, 0};
    return sys_exec(executable, arguments) < 0 ? -1 : 0;
}

static int is_elf_binary(const char* path)
{
    static const uint8_t elf_magic[4] = {0x7F, 'E', 'L', 'F'};
    uint8_t magic[sizeof(elf_magic)];
    ssize_t count = sys_read_at(path, magic, sizeof(magic), 0);
    if (count < 0) return -1;
    return count == (ssize_t)sizeof(magic) &&
           memcmp(magic, elf_magic, sizeof(elf_magic)) == 0;
}

static int confirm_binary_execution(const char* path)
{
    int key;
    createos_ui_clear();
    sys_puts("  EXECUTABLE FILE\n\n", 0x0C);
    sys_puts("  This file is a native ELF binary, not a .APPL application.\n",
             0x0E);
    sys_puts("  Explorer confirmation is only an accidental-launch warning;\n",
             0x07);
    sys_puts("  it does not sandbox or verify this program.\n\n", 0x07);
    sys_puts("  File: ", 0x0F);
    sys_puts(path, 0x0B);
    sys_puts("\n\n  Run this binary? [y/N] ", 0x0E);
    do {
        key = createos_ui_read_key();
    } while (key != 'y' && key != 'Y' && key != 'n' && key != 'N' &&
             key != 27 && key != KEY_INTERRUPT && key != KEY_CTRL_C);
    screen_initialized = 0;
    return key == 'y' || key == 'Y';
}

static int launch_editor(explorer_state_t* state, const char* path)
{
    char previous_cwd[EXPLORER_PATH_MAX];
    set_pointer_mode(0);
    if (sys_getcwd(previous_cwd, sizeof(previous_cwd)) < 0)
        strcpy(previous_cwd, "/");
    if (sys_chdir(state->path) < 0) return -1;
    if (sys_app_return_explorer(1) < 0) {
        (void)sys_chdir(previous_cwd);
        return -1;
    }
    if (launch_path("/app/system/bin/textedit.APPL", path) < 0) {
        (void)sys_app_return_explorer(0);
        (void)sys_chdir(previous_cwd);
        return -1;
    }
    return 0;
}

static int activate_selected(explorer_state_t* state)
{
    char selected_path[EXPLORER_PATH_MAX];
    fs_stat_t status;
    if (state->selected < 0 || state->selected >= state->count) return 0;
    if (path_join(selected_path, sizeof(selected_path), state->path,
                  state->names[state->selected]) < 0 ||
        sys_stat(selected_path, &status) < 0)
        return -1;
    if (status.type == FS_TYPE_DIR)
        return load_directory(state, selected_path);
    if (status.type != FS_TYPE_FILE) return -1;
    int is_binary = is_elf_binary(selected_path);
    if (is_binary < 0) return -1;
    if (has_appl_suffix(selected_path)) {
        char application[EXPLORER_PATH_MAX];
        if (resolve_user_application_link(selected_path, application,
                                           sizeof(application))) {
            set_pointer_mode(0);
            return launch_path(application, 0);
        }
        if (is_installed_application(selected_path)) {
            set_pointer_mode(0);
            return launch_path(selected_path, 0);
        }
    }
    if (is_binary) {
        if (confirm_binary_execution(selected_path)) {
            set_pointer_mode(0);
            return launch_path(selected_path, 0);
        } else {
            return 0;
        }
    }
    if (has_appl_suffix(selected_path))
        return -1;
    return launch_editor(state, selected_path);
}

static int go_parent(explorer_state_t* state)
{
    char parent[EXPLORER_PATH_MAX];
    size_t length = strlen(state->path);
    if (length <= 1) return 0;
    memcpy(parent, state->path, length + 1);
    while (length > 1 && parent[length - 1] == '/') parent[--length] = 0;
    while (length > 1 && parent[length - 1] != '/') parent[--length] = 0;
    if (length > 1) parent[length - 1] = 0;
    if (!parent[0]) strcpy(parent, "/");
    return load_directory(state, parent);
}

static void draw_explorer(const explorer_state_t* state)
{
    char (*lines)[EXPLORER_MAX_COLUMNS + 1] = screen_lines;
    uint8_t* colors = screen_colors;
    char text[EXPLORER_MAX_COLUMNS + 1];
    size_t footer_row = explorer_screen_rows - 5;
    int visible_count = state->count - state->first_visible;
    if (visible_count > (int)explorer_visible_items)
        visible_count = (int)explorer_visible_items;

    for (size_t row = 0; row < explorer_screen_rows; row++)
        colors[row] = 0x07;
    format_screen_line(lines[0], "  CREATEOS  /  FILE EXPLORER");
    colors[0] = 0x0B;
    snprintf(text, sizeof(text), "  %s  -  %d item(s)", state->path,
             state->count);
    format_screen_line(lines[1], text);
    colors[1] = 0x0F;
    format_screen_line(lines[2],
        "  [ OPEN ] [ UP ] [ NEW DIR ] [ NEW FILE ] [ QUIT ]");
    colors[2] = toolbar_hover >= 0 ? 0x0F : 0x0E;
    format_screen_line(lines[3],
        "  >>> DROP .APPL HERE TO INSTALL IN YOUR PERSONAL APP FOLDER <<<");
    colors[3] = drop_target_hover ? 0x1F : 0x0B;
    for (int row = 0; row < visible_count; row++) {
        int index = state->first_visible + row;
        char item_path[EXPLORER_PATH_MAX];
        fs_stat_t status;
        int is_dir = path_join(item_path, sizeof(item_path), state->path,
                               state->names[index]) == 0 &&
                     sys_stat(item_path, &status) == 0 &&
                     status.type == FS_TYPE_DIR;
        const char* kind = is_dir ? "DIR " :
            has_appl_suffix(state->names[index]) ? "APP " :
            has_source_suffix(state->names[index]) ? "CODE" : "FILE";
        snprintf(text, sizeof(text), "    %s  %s", kind,
                 state->names[index]);
        format_screen_line(lines[row + 4], text);
        if (index == state->selected) {
            snprintf(text, sizeof(text), "  > %s  %s", kind,
                     state->names[index]);
            format_screen_line(lines[row + 4], text);
            colors[row + 4] = 0x0F;
        } else if ((index & 1) == 0) {
            colors[row + 4] = 0x08;
        }
    }
    for (int row = visible_count; row < (int)explorer_visible_items; row++)
        format_screen_line(lines[row + 4], "");
    format_screen_line(lines[footer_row],
                       "-------------------------------------------------------------------------------");
    colors[footer_row] = 0x08;
    if (state->selected >= 0 && state->selected < state->count) {
        char item_path[EXPLORER_PATH_MAX];
        fs_stat_t status;
        const char* kind = "FILE";
        if (path_join(item_path, sizeof(item_path), state->path,
                      state->names[state->selected]) == 0 &&
            sys_stat(item_path, &status) == 0) {
            if (status.type == FS_TYPE_DIR) kind = "DIRECTORY";
            else if (has_appl_suffix(state->names[state->selected]))
                kind = "APPLICATION";
            else if (has_source_suffix(state->names[state->selected]))
                kind = "SOURCE FILE";
            snprintf(text, sizeof(text), "  Selected: %s  -  %s  -  %d bytes",
                     state->names[state->selected], kind, (int)status.size);
        } else {
            snprintf(text, sizeof(text), "  Selected: %s",
                     state->names[state->selected]);
        }
    } else {
        snprintf(text, sizeof(text), "  This directory is empty");
    }
    format_screen_line(lines[footer_row + 1], text);
    colors[footer_row + 1] = 0x0B;
    format_screen_line(lines[footer_row + 2],
        "  Enter Open/Edit/Run     A Your Apps       Backspace Up");
    colors[footer_row + 2] = 0x0E;
    format_screen_line(lines[footer_row + 3],
        "  N New folder  F New file    Drag .APPL to install");
    colors[footer_row + 3] = 0x0E;
    snprintf(text, sizeof(text), "  %s",
             explorer_message ? explorer_message : "Ready");
    format_screen_line(lines[footer_row + 4], text);
    colors[footer_row + 4] = explorer_message ? 0x0E : 0x0A;

    for (size_t row = 0; row < explorer_screen_rows; row++) {
        if (screen_initialized &&
            colors[row] == previous_colors[row] &&
            strcmp(lines[row], previous_screen[row]) == 0)
            continue;
        (void)createos_ui_write_at(&explorer_ui, 0, (uint32_t)row,
                                   lines[row], colors[row]);
    }
    memcpy(previous_screen, lines,
           explorer_screen_rows * sizeof(previous_screen[0]));
    memcpy(previous_colors, colors, explorer_screen_rows);
    screen_initialized = 1;
}

static void draw_name_dialog(const char* title, const char* prompt,
                             const char* name, size_t length)
{
    char line[EXPLORER_MAX_COLUMNS + 1];
    size_t box_width = explorer_screen_columns > 58
        ? 56 : explorer_screen_columns - 2;
    size_t left = (explorer_screen_columns - box_width) / 2;
    size_t top = (explorer_screen_rows - 7) / 2;
    size_t prompt_length = strlen(prompt);
    size_t visible_length = length;
    if (box_width > sizeof(line) - 1)
        box_width = sizeof(line) - 1;

    memset(line, ' ', explorer_screen_columns);
    line[explorer_screen_columns] = 0;
    for (size_t row = 0; row < 7; row++) {
        size_t right = left + box_width - 1;
        line[left] = (row == 0 || row == 6) ? '+' : '|';
        line[right] = (row == 0 || row == 6) ? '+' : '|';
        if (row == 0 || row == 6)
            memset(line + left + 1, '-', box_width - 2);
        else if (row == 2)
            memset(line + left + 1, '-', box_width - 2);
        else if (row == 1 || row == 5) {
            const char* label = row == 1 ? title : "Enter: create   Esc: cancel";
            size_t label_length = strlen(label);
            size_t label_column = left + (box_width - label_length) / 2;
            memcpy(line + label_column, label, label_length);
        } else if (row == 3) {
            size_t available = box_width - 4;
            size_t shown_prompt = prompt_length < available
                ? prompt_length : available;
            memcpy(line + left + 2, prompt, shown_prompt);
            size_t shown_name = visible_length;
            if (shown_name > available - shown_prompt)
                shown_name = available - shown_prompt;
            if (shown_name)
                memcpy(line + left + 2 + shown_prompt, name, shown_name);
        }
        (void)sys_set_cursor(0, top + row);
        sys_puts(line, row == 0 || row == 6 ? 0x0B : 0x0F);
        memset(line + left, ' ', box_width);
    }
    size_t input_column = left + 2 + prompt_length + length;
    if (input_column >= left + box_width - 1)
        input_column = left + box_width - 2;
    (void)sys_set_cursor((int)input_column, top + 3);
}

static int read_name(char* name, size_t capacity, int directory)
{
    size_t length = 0;
    const char* title = directory ? "CREATE NEW FOLDER" : "CREATE NEW TEXT FILE";
    const char* prompt = directory ? "Folder name: " : "File name: ";
    draw_name_dialog(title, prompt, name, length);
    for (;;) {
        int key = createos_ui_read_key();
        if (key == '\r' || key == '\n') {
            name[length] = 0;
            return length ? 0 : -1;
        }
        if (key == 27) {
            name[0] = 0;
            return -1;
        }
        if (key == 8 || key == 127) {
            if (length) {
                length--;
                name[length] = 0;
            }
            draw_name_dialog(title, prompt, name, length);
            continue;
        }
        if (key < 33 || key > 126 || key == '/' || key == '\\' ||
            key == ':' || key == '*' || key == '?' || key == '"' ||
            key == '<' || key == '>' || key == '|')
            continue;
        if (length + 1 >= capacity) continue;
        name[length++] = (char)key;
        name[length] = 0;
        draw_name_dialog(title, prompt, name, length);
    }
}

static int create_entry(explorer_state_t* state, int directory)
{
    char name[USER_NAME_MAX];
    char path[EXPLORER_PATH_MAX];
    if (read_name(name, sizeof(name), directory) < 0) {
        screen_initialized = 0;
        return 0;
    }
    screen_initialized = 0;
    if (path_join(path, sizeof(path), state->path, name) < 0)
        return -1;
    if (directory) return sys_mkdir(path, 0755);
    if (sys_file_replace(path, 0, 0) < 0) return -1;
    return launch_editor(state, path);
}

static int toolbar_action_at(int column)
{
    static const char* const labels[] = {
        "OPEN", "UP", "NEW DIR", "NEW FILE", "QUIT"
    };
    int start = 2;
    for (int i = 0; i < (int)(sizeof(labels) / sizeof(labels[0])); i++) {
        int end = start + (int)strlen(labels[i]) + 4;
        if (column >= start && column < end) return i;
        start = end + 1;
    }
    return -1;
}

static void update_mouse_hover(explorer_state_t* state)
{
    int position = createos_ui_mouse_position();
    int previous_selection = state->selected;
    int over_item = 0;
    mouse_x = position & 0xFFFF;
    mouse_y = ((unsigned int)position >> 16) & 0xFFFF;
    drop_target_hover = mouse_dragging && mouse_y == 3;
    toolbar_hover = mouse_y == 2 ? toolbar_action_at(mouse_x) : -1;
    if (mouse_y >= 4 &&
        mouse_y < 4 + (int)explorer_visible_items) {
        int selected = state->first_visible + mouse_y - 4;
        if (selected < state->count) {
            state->selected = selected;
            over_item = 1;
        }
    }
    if (pointer_supported)
        set_pointer_mode(toolbar_hover >= 0 || over_item ||
                         drop_target_hover ? 1 : 0);
    if (previous_selection != state->selected)
        keep_selection_visible(state);
}

static int handle_mouse_click(explorer_state_t* state)
{
    int action;
    update_mouse_hover(state);
    action = mouse_y == 2 ? toolbar_action_at(mouse_x) : -1;
    if (action < 0 && mouse_y >= 4 &&
        mouse_y < 4 + (int)explorer_visible_items) {
        int selected = state->first_visible + mouse_y - 4;
        if (selected >= 0 && selected < state->count) {
            state->selected = selected;
            return activate_selected(state);
        }
        return 0;
    }
    if (action == 0) return activate_selected(state);
    if (action == 1) return go_parent(state);
    if (action == 2) return create_entry(state, 1);
    if (action == 3) return create_entry(state, 0);
    if (action == 4) {
        set_pointer_mode(0);
        createos_ui_clear();
        (void)createos_ui_shutdown();
        sys_exit(0);
    }
    return 0;
}

static void keep_selection_visible(explorer_state_t* state)
{
    if (state->selected < state->first_visible)
        state->first_visible = state->selected;
    else if (state->selected >= state->first_visible + (int)explorer_visible_items)
        state->first_visible =
            state->selected - (int)explorer_visible_items + 1;
}

int main(int argc, char* argv[])
{
    explorer_state_t state;
    char cwd[EXPLORER_PATH_MAX];
    int result;
    if (argc > 2) {
        sys_puts("Usage: explorer.APPL [directory]\n", 0x0E);
        return 2;
    }
    if (argc == 2) {
        if (load_directory(&state, argv[1]) < 0) {
            printf("explorer: not a directory or access denied: %s\n", argv[1]);
            return 1;
        }
    } else {
        if (sys_getcwd(cwd, sizeof(cwd)) < 0) strcpy(cwd, "/");
        if (load_directory(&state, cwd) < 0) {
            sys_puts("explorer: current directory could not be listed\n", 0x0C);
            return 1;
        }
    }

    explorer_detect_screen_size();
    screen_initialized = 0;
    if (createos_ui_init(&explorer_ui) < 0)
        return 1;

    for (;;) {
        int key;
        update_mouse_hover(&state);
        draw_explorer(&state);
        key = createos_ui_read_key();
        if (key == 'q' || key == 27) {
            set_pointer_mode(0);
            createos_ui_clear();
            (void)createos_ui_shutdown();
            return 0;
        }
        if (key == 'a' || key == 'A') {
            char apps_directory[EXPLORER_PATH_MAX];
            result = user_applications_directory(
                apps_directory, sizeof(apps_directory), 1) < 0 ? -1 :
                load_directory(&state, apps_directory);
            explorer_message = result < 0
                ? "Your applications directory could not be opened." : 0;
            screen_initialized = 0;
        }
        if (key == KEY_UP && state.selected > 0) state.selected--;
        else if (key == KEY_DOWN && state.selected + 1 < state.count)
            state.selected++;
        else if (key == KEY_LEFT || key == 8 || key == 127) {
            result = go_parent(&state);
            explorer_message = result < 0
                ? "Parent directory could not be opened." : 0;
        } else if (key == '\r' || key == '\n' || key == KEY_RIGHT) {
            result = activate_selected(&state);
            explorer_message = result < 0
                ? "Item could not be opened." : 0;
        } else if (key == 'n' || key == 'f') {
            result = create_entry(&state, key == 'n');
            explorer_message = result < 0 ? "Creation failed." :
                key == 'n' ? "Folder created." : "Text file created.";
            screen_initialized = 0;
            if (load_directory(&state, state.path) < 0) return 1;
        } else if (key == KEY_MOUSE_LEFT) {
            begin_mouse_action(&state);
        } else if (key == KEY_MOUSE_DRAG) {
            if (mouse_pressed)
                mouse_dragging = 1;
        } else if (key == KEY_MOUSE_RELEASE) {
            if (mouse_pressed)
                finish_mouse_action(&state);
        } else if (key == KEY_MOUSE_UP || key == KEY_MOUSE_DOWN) {
            if (state.count > 0) {
                int step = key == KEY_MOUSE_UP ? -3 : 3;
                state.selected += step;
                if (state.selected < 0) state.selected = 0;
                if (state.selected >= state.count)
                    state.selected = state.count - 1;
            }
        } else if (key == KEY_MOUSE_MOVE) {
            explorer_message = 0;
        }
        if (key == KEY_UP || key == KEY_DOWN) explorer_message = 0;
        keep_selection_visible(&state);
    }
}