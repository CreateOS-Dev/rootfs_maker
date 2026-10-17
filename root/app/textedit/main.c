#include "user_syscall.h"
#include "createos/uikit.h"

#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#undef snprintf
int snprintf(char* output, size_t capacity, const char* format, ...);

#define EDITOR_CAPACITY 4096
#define EDITOR_MAX_ROWS 100
#define EDITOR_MAX_COLUMNS 160
#define EDITOR_MAX_TABS 8
#define EDITOR_PATH_MAX 256
#define EDITOR_TREE_MAX_ITEMS 64
#define EDITOR_TREE_WIDTH 22
#define TEXTEDIT_CC_PATH "/app/system/bin/tcc.APPL"

typedef struct {
    char data[EDITOR_CAPACITY + 1];
    size_t length;
    size_t cursor;
    size_t selection_anchor;
    size_t first_line;
    int dirty;
    int selection_active;
    int cursor_type;
    const char* message;
} editor_state_t;

typedef struct {
    editor_state_t editor;
    char path[EDITOR_PATH_MAX];
} editor_tab_t;

static editor_tab_t editor_tabs[EDITOR_MAX_TABS];
static int editor_tab_count;
static int editor_active_tab;
static char screen_lines[EDITOR_MAX_ROWS][EDITOR_MAX_COLUMNS + 1];
static char previous_screen[EDITOR_MAX_ROWS][EDITOR_MAX_COLUMNS + 1];
static uint8_t screen_colors[EDITOR_MAX_ROWS];
static uint8_t previous_colors[EDITOR_MAX_ROWS];
static uint8_t screen_glyph_colors[EDITOR_MAX_ROWS][EDITOR_MAX_COLUMNS];
static uint8_t previous_glyph_colors[EDITOR_MAX_ROWS][EDITOR_MAX_COLUMNS];
static int screen_initialized;
static int editor_graphical_console;
static size_t editor_screen_rows = 25;
static size_t editor_screen_columns = 79;
static size_t editor_visible_lines = 18;
static size_t editor_visible_columns = 76;
static char editor_clipboard[EDITOR_CAPACITY];
static size_t editor_clipboard_length;
static int pointer_mode = -1;
static char editor_tree_path[EDITOR_PATH_MAX] = "/";
static char editor_tree_names[EDITOR_TREE_MAX_ITEMS][USER_NAME_MAX];
static int editor_tree_count;
static int editor_tree_selected;
static int editor_tree_first;
static int editor_tree_visible = 1;
static int editor_tree_focused;
static editor_tab_t workspace_tab;

static int has_lua_suffix(const char* path);
static int suffix_equal(const char* path, const char* suffix);
static int save_text(editor_state_t* state, const char* path);

static size_t editor_text_offset(void)
{
    return editor_tree_visible ? EDITOR_TREE_WIDTH : 0;
}

static void update_editor_visible_columns(void)
{
    size_t offset = editor_text_offset();
    editor_visible_columns = editor_screen_columns > offset + 3
        ? editor_screen_columns - offset - 3 : editor_screen_columns;
}

static void update_pointer_mode(int y)
{
    size_t footer_row = editor_screen_rows - 3;
    int mode = y >= 5 && y < (int)footer_row ? 2 : 0;
    if (pointer_mode == mode)
        return;
    if (sys_tty_pointer_mode(mode) == 0)
        pointer_mode = mode;
}

static void editor_detect_screen_size(void)
{
    tty_info_t info;
    char tty_name[16] = { 0 };
    int serial_console = sys_tty_name(tty_name) == 0 &&
                         strncmp(tty_name, "ttyS", 4) == 0;
    editor_graphical_console = 0;
    if (!serial_console && sys_tty_info(4, &info) == 0 &&
        (info.flags & TTY_FLAG_GRAPHICAL) && info.width >= 80 &&
        info.height >= 25) {
        editor_screen_columns = info.width;
        editor_screen_rows = info.height;
        editor_graphical_console = 1;
    }
    if (editor_screen_columns >= EDITOR_MAX_COLUMNS)
        editor_screen_columns = EDITOR_MAX_COLUMNS;
    if (editor_screen_rows >= EDITOR_MAX_ROWS)
        editor_screen_rows = EDITOR_MAX_ROWS;
    editor_screen_columns--;
    update_editor_visible_columns();
    editor_visible_lines = editor_screen_rows > 8
        ? editor_screen_rows - 8 : 1;
}

static const char* path_basename(const char* path)
{
    const char* name = path;
    for (const char* cursor = path; *cursor; cursor++)
        if (*cursor == '/') name = cursor + 1;
    return name;
}

static int tree_load_directory(const char* path)
{
    char names[EDITOR_TREE_MAX_ITEMS][USER_NAME_MAX];
    fs_stat_t status;
    int count;

    if (!path || strlen(path) >= sizeof(editor_tree_path) ||
        sys_stat(path, &status) < 0 || status.type != FS_TYPE_DIR)
        return -1;
    count = sys_list(path, names, EDITOR_TREE_MAX_ITEMS);
    if (count < 0 || count > EDITOR_TREE_MAX_ITEMS)
        return -1;
    for (int i = 1; i < count; i++) {
        char current[USER_NAME_MAX];
        int j = i;
        strcpy(current, names[i]);
        while (j > 0 && strcmp(names[j - 1], current) > 0) {
            strcpy(names[j], names[j - 1]);
            j--;
        }
        strcpy(names[j], current);
    }
    strcpy(editor_tree_path, path);
    memcpy(editor_tree_names, names, (size_t)count * sizeof(names[0]));
    editor_tree_count = count;
    editor_tree_selected = 0;
    editor_tree_first = 0;
    return 0;
}

static void tree_initialize(const char* file_path)
{
    char directory[EDITOR_PATH_MAX];
    size_t length = strlen(file_path);
    const char* slash = 0;

    for (size_t i = 0; i < length; i++)
        if (file_path[i] == '/') slash = file_path + i;
    if (!slash)
        strcpy(directory, "/");
    else if (slash == file_path)
        strcpy(directory, "/");
    else {
        size_t directory_length = (size_t)(slash - file_path);
        memcpy(directory, file_path, directory_length);
        directory[directory_length] = 0;
    }
    if (tree_load_directory(directory) < 0)
        (void)tree_load_directory("/");
}

static int tree_initialize_directory(const char* path)
{
    if (tree_load_directory(path) == 0)
        return 0;
    return tree_load_directory("/");
}

static int resolve_path(const char* path, const char* base,
                        char* output, size_t capacity)
{
    size_t base_length;
    size_t path_length;
    size_t total_length;
    if (!path || !path[0] || !output || !capacity)
        return -1;
    if (path[0] == '/') {
        path_length = strlen(path);
        if (path_length >= capacity)
            return -1;
        memcpy(output, path, path_length + 1);
        while (path_length > 1 && output[path_length - 1] == '/')
            output[--path_length] = 0;
        return 0;
    }
    if (!base || base[0] != '/')
        return -1;
    base_length = strlen(base);
    path_length = strlen(path);
    if (base_length + (base_length > 1) + path_length >= capacity)
        return -1;
    memcpy(output, base, base_length);
    if (base_length > 1)
        output[base_length++] = '/';
    memcpy(output + base_length, path, path_length + 1);
    total_length = base_length + path_length;
    while (total_length > 1 && output[total_length - 1] == '/')
        output[--total_length] = 0;
    return 0;
}

static size_t tree_visible_rows(void)
{
    size_t footer_row = editor_screen_rows - 3;
    return footer_row > 5 ? footer_row - 5 : 1;
}

static void tree_keep_selection_visible(void)
{
    size_t rows = tree_visible_rows();
    int maximum = editor_tree_count;
    if (editor_tree_selected < 0) editor_tree_selected = 0;
    if (editor_tree_selected > maximum) editor_tree_selected = maximum;
    if (editor_tree_selected < editor_tree_first)
        editor_tree_first = editor_tree_selected;
    if ((size_t)(editor_tree_selected - editor_tree_first) >= rows)
        editor_tree_first = editor_tree_selected - (int)rows + 1;
}

static void draw_tree_row(size_t row)
{
    char text[EDITOR_TREE_WIDTH];
    fs_stat_t status;
    size_t offset = editor_text_offset();
    size_t item = (size_t)editor_tree_first + row - 5;
    uint8_t color = 0x08;

    memset(text, ' ', sizeof(text));
    text[sizeof(text) - 1] = 0;
    if (item <= (size_t)editor_tree_count) {
        const char* name;
        int is_directory = 1;
        if (item == 0) {
            name = "../";
            if (strcmp(editor_tree_path, "/") == 0)
                name = "./";
        } else {
            char child[EDITOR_PATH_MAX];
            size_t path_length = strlen(editor_tree_path);
            size_t name_length;
            name = editor_tree_names[item - 1];
            name_length = strlen(name);
            if (path_length + (path_length > 1) + name_length <
                sizeof(child)) {
                memcpy(child, editor_tree_path, path_length);
                if (path_length > 1) child[path_length++] = '/';
                memcpy(child + path_length, name, name_length + 1);
                is_directory = sys_stat(child, &status) == 0 &&
                               status.type == FS_TYPE_DIR;
            }
            if (is_directory) {
                size_t length = strlen(name);
                if (length < sizeof(text) - 2) {
                    memcpy(text + 2, name, length);
                    text[2 + length] = '/';
                }
            }
        }
        if (item == 0 || !text[2]) {
            size_t length = strlen(name);
            size_t capacity = sizeof(text) - 3;
            if (length > capacity) length = capacity;
            memcpy(text + 2, name, length);
        }
        text[0] = item == (size_t)editor_tree_selected ? '>' : ' ';
        text[1] = ' ';
        color = item == (size_t)editor_tree_selected
            ? editor_tree_focused ? 0x1F : 0x17
            : 0x07;
    }
    memcpy(screen_lines[row], text, sizeof(text));
    memset(screen_glyph_colors[row], 0x08, offset);
    screen_lines[row][EDITOR_TREE_WIDTH - 1] = '|';
    screen_glyph_colors[row][EDITOR_TREE_WIDTH - 1] = 0x08;
    for (size_t column = 0; column < sizeof(text) - 1; column++)
        if (text[column] != ' ')
            screen_glyph_colors[row][column] = color;
}

static int has_source_suffix(const char* path)
{
    size_t length = strlen(path);
    return (length >= 2 && path[length - 2] == '.' &&
            (path[length - 1] == 'c' || path[length - 1] == 'C')) ||
           (length >= 2 && path[length - 2] == '.' &&
            (path[length - 1] == 'h' || path[length - 1] == 'H'));
}

static int is_identifier_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

static int is_c_keyword(const char* word, size_t length)
{
    static const char* const keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch",
        "typedef", "union", "unsigned", "void", "volatile", "while",
        "_Bool", "_Complex", "_Imaginary"
    };
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (strlen(keywords[i]) == length &&
            strncmp(word, keywords[i], length) == 0)
            return 1;
    }
    return 0;
}

static int c_block_comment_before_line(const editor_state_t* state,
                                       size_t target_line)
{
    size_t line = 0;
    int in_comment = 0;
    char quote = 0;
    int escaped = 0;

    for (size_t i = 0; i < state->length && line < target_line; i++) {
        char ch = state->data[i];
        char next = i + 1 < state->length ? state->data[i + 1] : 0;
        if (ch == '\n') {
            line++;
            quote = 0;
            escaped = 0;
            continue;
        }
        if (in_comment) {
            if (ch == '*' && next == '/') {
                in_comment = 0;
                i++;
            }
        } else if (quote) {
            if (escaped) escaped = 0;
            else if (ch == '\\') escaped = 1;
            else if (ch == quote) quote = 0;
        } else if ((ch == '/' && next == '/') ) {
            while (i < state->length && state->data[i] != '\n') i++;
            if (i < state->length) line++;
        } else if (ch == '/' && next == '*') {
            in_comment = 1;
            i++;
        } else if (ch == '"' || ch == '\'') {
            quote = ch;
        }
    }
    return in_comment;
}

static void color_c_line(const char* line,
                         uint8_t colors[EDITOR_MAX_COLUMNS],
                         int in_block_comment)
{
    size_t length = editor_visible_columns;
    size_t i = 0;
    int first_token = 1;

    while (length && line[length - 1] == ' ') length--;
    while (i < length) {
        size_t start = i;
        char quote;
        if (in_block_comment) {
            while (i < length) {
                if (line[i] == '*' && i + 1 < length &&
                    line[i + 1] == '/') {
                    i += 2;
                    in_block_comment = 0;
                    break;
                }
                i++;
            }
            for (size_t j = start; j < i; j++) colors[j] = 0x08;
            continue;
        }
        if (line[i] == ' ' || line[i] == '\t') {
            i++;
            continue;
        }
        if (line[i] == '#' && first_token) {
            for (; i < length; i++) colors[i] = 0x0B;
            break;
        }
        first_token = 0;
        if (line[i] == '/' && i + 1 < length && line[i + 1] == '/') {
            for (; i < length; i++) colors[i] = 0x08;
            break;
        }
        if (line[i] == '/' && i + 1 < length && line[i + 1] == '*') {
            in_block_comment = 1;
            continue;
        }
        if (line[i] == '"' || line[i] == '\'') {
            quote = line[i++];
            while (i < length) {
                if (line[i] == '\\' && i + 1 < length) {
                    i += 2;
                    continue;
                }
                if (line[i++] == quote) break;
            }
            for (size_t j = start; j < i; j++) colors[j] = 0x0E;
            continue;
        }
        if ((line[i] >= '0' && line[i] <= '9') ||
            (line[i] == '.' && i + 1 < length &&
             line[i + 1] >= '0' && line[i + 1] <= '9')) {
            i++;
            while (i < length &&
                   (is_identifier_char(line[i]) || line[i] == '.'))
                i++;
            for (size_t j = start; j < i; j++) colors[j] = 0x0D;
            continue;
        }
        if ((line[i] >= 'a' && line[i] <= 'z') ||
            (line[i] >= 'A' && line[i] <= 'Z') || line[i] == '_') {
            while (i < length && is_identifier_char(line[i])) i++;
            if (is_c_keyword(line + start, i - start))
                for (size_t j = start; j < i; j++) colors[j] = 0x0B;
            continue;
        }
        i++;
    }
}

static void format_screen_line(char* output, const char* text)
{
    size_t length = strlen(text);
    if (length > editor_screen_columns)
        length = editor_screen_columns;
    memset(output, ' ', editor_screen_columns);
    memcpy(output, text, length);
    output[editor_screen_columns] = 0;
}

static int insert_bytes(editor_state_t* state, const char* bytes, size_t count)
{
    if (count > EDITOR_CAPACITY - state->length) return 0;
    memmove(state->data + state->cursor + count,
            state->data + state->cursor,
            state->length - state->cursor);
    memcpy(state->data + state->cursor, bytes, count);
    state->cursor += count;
    state->length += count;
    state->data[state->length] = 0;
    state->dirty = 1;
    return 1;
}

static void erase_bytes(editor_state_t* state, size_t start, size_t count)
{
    if (start > state->length || count > state->length - start) return;
    memmove(state->data + start, state->data + start + count,
            state->length - start - count);
    state->length -= count;
    state->data[state->length] = 0;
    state->dirty = 1;
}

static int selection_bounds(const editor_state_t* state,
                            size_t* start, size_t* end)
{
    if (!state->selection_active ||
        state->selection_anchor == state->cursor)
        return 0;
    *start = state->selection_anchor < state->cursor
        ? state->selection_anchor : state->cursor;
    *end = state->selection_anchor > state->cursor
        ? state->selection_anchor : state->cursor;
    return 1;
}

static void set_cursor(editor_state_t* state, size_t position, int extend)
{
    if (extend) {
        if (!state->selection_active) {
            state->selection_anchor = state->cursor;
            state->selection_active = 1;
        }
    } else {
        state->selection_active = 0;
    }
    state->cursor = position;
}

static int delete_selection(editor_state_t* state)
{
    size_t start, end;
    if (!selection_bounds(state, &start, &end)) {
        state->selection_active = 0;
        return 0;
    }
    erase_bytes(state, start, end - start);
    state->cursor = start;
    state->selection_active = 0;
    return 1;
}

static int copy_selection(const editor_state_t* state)
{
    size_t start, end;
    if (!selection_bounds(state, &start, &end)) return 0;
    editor_clipboard_length = end - start;
    memcpy(editor_clipboard, state->data + start, editor_clipboard_length);
    return sys_clipboard_set(editor_clipboard, editor_clipboard_length) == 0;
}

static int replace_selection(editor_state_t* state,
                             const char* bytes, size_t count)
{
    size_t start, end;
    if (selection_bounds(state, &start, &end)) {
        size_t new_length = state->length - (end - start);
        if (count > EDITOR_CAPACITY - new_length) return 0;
        erase_bytes(state, start, end - start);
        state->cursor = start;
        state->selection_active = 0;
    }
    return insert_bytes(state, bytes, count);
}

static void color_selection(uint8_t* colors,
                            const size_t source_positions[EDITOR_MAX_COLUMNS],
                            size_t selection_start, size_t selection_end)
{
    if (selection_start == selection_end) return;
    for (size_t column = 0; column < editor_visible_columns; column++) {
        size_t source = source_positions[column];
        if (source >= selection_start && source < selection_end)
            colors[column] = 0x1F;
    }
}

static size_t line_start(const editor_state_t* state, size_t position)
{
    while (position && state->data[position - 1] != '\n') position--;
    return position;
}

static size_t line_end(const editor_state_t* state, size_t position)
{
    while (position < state->length && state->data[position] != '\n')
        position++;
    return position;
}

static size_t logical_column(const editor_state_t* state, size_t position)
{
    size_t start = line_start(state, position);
    size_t column = 0;
    while (start < position) {
        if (state->data[start] == '\t')
            column += 4 - (column % 4);
        else
            column++;
        start++;
    }
    return column;
}

static size_t line_number(const editor_state_t* state, size_t position)
{
    size_t line = 0;
    for (size_t i = 0; i < position; i++)
        if (state->data[i] == '\n') line++;
    return line;
}

static size_t position_at_column(const editor_state_t* state,
                                 size_t start, size_t end,
                                 size_t target_column)
{
    size_t column = 0;
    size_t position = start;
    while (position < end) {
        size_t next_column = state->data[position] == '\t'
            ? column + 4 - (column % 4)
            : column + 1;
        if (target_column < next_column) {
            if (state->data[position] == '\t' &&
                target_column - column >=
                    (next_column - column + 1) / 2)
                return position + 1;
            return position;
        }
        column = next_column;
        position++;
    }
    return position;
}

static void move_vertical(editor_state_t* state, int direction)
{
    size_t current_start = line_start(state, state->cursor);
    size_t column = logical_column(state, state->cursor);
    size_t target_start;
    size_t target_end;
    if (direction < 0) {
        if (!current_start) return;
        target_end = current_start - 1;
        target_start = line_start(state, target_end);
    } else {
        size_t current_end = line_end(state, state->cursor);
        if (current_end == state->length) return;
        target_start = current_end + 1;
        target_end = line_end(state, target_start);
    }
    state->cursor = position_at_column(state, target_start, target_end,
                                       column);
}

static void draw_line(char* output, const editor_state_t* state,
                      size_t line_index, size_t cursor_line,
                      size_t cursor_column, size_t* screen_cursor_column,
                      size_t source_positions[EDITOR_MAX_COLUMNS])
{
    static const char cursor_chars[] = {'|', '_'};
    const int cursor_visible = editor_graphical_console &&
                               ((int)(sys_ticks() / 25u) & 1u);
    memset(output, ' ', editor_screen_columns);
    for (size_t i = 0; i < EDITOR_MAX_COLUMNS; i++)
        source_positions[i] = (size_t)-1;
    size_t start = 0;
    size_t line = 0;
    size_t logical = 0;
    size_t visible = 0;
    size_t end;
    size_t horizontal = line_index == cursor_line &&
                        cursor_column >= editor_visible_columns
        ? cursor_column - editor_visible_columns + 1 : 0;

    while (line < line_index && start < state->length) {
        if (state->data[start++] == '\n') line++;
    }
    end = line_end(state, start);
    while (start <= end) {
        if (line_index == cursor_line && start == state->cursor &&
            logical >= horizontal && visible < editor_visible_columns) {
            if (screen_cursor_column)
                *screen_cursor_column = visible;
            if (cursor_visible)
                output[visible] = cursor_chars[state->cursor_type & 1];
            visible++;
        }
        if (start == end) break;
        if (state->data[start] == '\t') {
            size_t spaces = 4 - (logical % 4);
            for (size_t i = 0; i < spaces; i++) {
                if (logical >= horizontal && visible < editor_visible_columns) {
                    output[visible] = ' ';
                    source_positions[visible] = start;
                    visible++;
                }
                logical++;
            }
        } else {
            if (logical >= horizontal && visible < editor_visible_columns) {
                output[visible] = state->data[start];
                source_positions[visible] = start;
                visible++;
            }
            logical++;
        }
        start++;
    }
    if (line_index == cursor_line && state->cursor == end &&
        logical >= horizontal && visible < editor_visible_columns) {
        if (screen_cursor_column)
            *screen_cursor_column = visible;
        if (cursor_visible)
            output[visible++] = cursor_chars[state->cursor_type & 1];
    }
    while (visible < editor_visible_columns)
        output[visible++] = ' ';
    output[editor_screen_columns] = 0;
}

static size_t append_tab_label(char* output, size_t used, size_t capacity,
                               int tab_index)
{
    const editor_tab_t* tab = &editor_tabs[tab_index];
    const char* name = path_basename(tab->path);
    int written;
    if (used >= capacity - 1)
        return capacity;
    written = snprintf(output + used, capacity - used, "%s%d:%s%s",
                           tab_index == editor_active_tab ? " >" : "  ",
                           tab_index + 1, name,
                           tab->editor.dirty ? "*" : "");
    if (written < 0 || (size_t)written >= capacity - used)
        return capacity;
    return used + (size_t)written;
}

static void draw_editor(const editor_state_t* state, const char* path)
{
    char (*lines)[EDITOR_MAX_COLUMNS + 1] = screen_lines;
    uint8_t* colors = screen_colors;
    uint8_t (*glyph_colors)[EDITOR_MAX_COLUMNS] = screen_glyph_colors;
    char text[EDITOR_MAX_COLUMNS + 1];
    int source_file = has_source_suffix(path);
    size_t current_line = line_number(state, state->cursor);
    size_t current_column = logical_column(state, state->cursor);
    size_t total_lines = line_number(state, state->length) + 1;
    size_t screen_cursor_column = 0;
    size_t footer_row = editor_screen_rows - 3;
    size_t tab_labels_length = 0;
    size_t text_offset = editor_text_offset();
    size_t selected_start = 0;
    size_t selected_end = 0;
    int has_selection = selection_bounds(state, &selected_start, &selected_end);

    for (size_t row = 0; row < editor_screen_rows; row++) {
        colors[row] = 0x07;
        memset(glyph_colors[row], 0x07, editor_screen_columns);
    }
    snprintf(text, sizeof(text), "  CREATEOS  /  TEXTEDIT  -  %s%s",
             path_basename(path), state->dirty ? "  *" : "");
    format_screen_line(lines[0], text);
    colors[0] = 0x0B;
    snprintf(text, sizeof(text), "  FILE  %s  -  %s",
             path, state->dirty ? "UNSAVED" : "SAVED");
    format_screen_line(lines[1], text);
    colors[1] = 0x0F;
    for (int i = 0; i < editor_tab_count; i++)
        tab_labels_length = append_tab_label(text, tab_labels_length,
                                             sizeof(text), i);
    format_screen_line(lines[2], text);
    colors[2] = 0x0B;
    format_screen_line(lines[3], !editor_tab_count
        ? "  Ctrl+B Files   Enter Open   Ctrl+O Path   Esc Back"
        : suffix_equal(path, ".c")
            ? "  Ctrl+R Build+Run C   Ctrl+B Files   Ctrl+O Path   Ctrl+S Save   Esc Back"
        : has_lua_suffix(path)
            ? "  Ctrl+R Run Lua   Ctrl+B Files   Ctrl+O Path   Ctrl+S Save   Esc Back"
            : "  Ctrl+R N/A   Ctrl+B Files   Ctrl+O Path   Ctrl+S Save   Esc Back");
    colors[3] = 0x0E;
    format_screen_line(lines[4],
        "-------------------------------------------------------------------------------");
    colors[4] = 0x08;
    if (editor_tree_visible) {
        memcpy(lines[4], " FILES", 6);
        for (size_t column = 0; column < EDITOR_TREE_WIDTH; column++)
            glyph_colors[4][column] = column == EDITOR_TREE_WIDTH - 1
                ? 0x08 : 0x0B;
        lines[4][EDITOR_TREE_WIDTH - 1] = '|';
        for (size_t column = EDITOR_TREE_WIDTH;
             column < editor_screen_columns; column++)
            lines[4][column] = '-';
    }
    for (size_t row = 0; row < editor_visible_lines; row++) {
        size_t line = state->first_line + row;
        size_t source_positions[EDITOR_MAX_COLUMNS];
        if (line < total_lines)
            draw_line(lines[row + 5], state, line, current_line,
                      current_column,
                      line == current_line ? &screen_cursor_column : 0,
                      source_positions);
        else
            format_screen_line(lines[row + 5], "");
        if (source_file && row + 5 < footer_row)
            color_c_line(lines[row + 5], glyph_colors[row + 5],
                         c_block_comment_before_line(state, line));
        if (has_selection && line < total_lines)
            color_selection(glyph_colors[row + 5], source_positions,
                            selected_start, selected_end);
        if (text_offset) {
            memmove(lines[row + 5] + text_offset, lines[row + 5],
                    editor_visible_columns);
            memset(lines[row + 5], ' ', text_offset);
            memmove(glyph_colors[row + 5] + text_offset,
                    glyph_colors[row + 5], editor_visible_columns);
            memset(glyph_colors[row + 5], 0x08, text_offset);
        }
        if (editor_tree_visible && row + 5 < footer_row)
            draw_tree_row(row + 5);
    }

    format_screen_line(lines[footer_row],
        "-------------------------------------------------------------------------------");
    colors[footer_row] = 0x08;
    snprintf(text, sizeof(text), "  Line %d  Column %d  %d / %d bytes",
             (int)(current_line + 1),
             (int)(current_column + 1),
             (int)state->length, EDITOR_CAPACITY);
    format_screen_line(lines[footer_row + 1], text);
    colors[footer_row + 1] = 0x0B;
    snprintf(text, sizeof(text), "  %s",
             state->message ? state->message :
             state->dirty ? "Unsaved changes" : "Ready");
    format_screen_line(lines[footer_row + 2], text);
    colors[footer_row + 2] = state->message ? 0x0E : 0x0A;
    for (size_t row = 0; row < editor_screen_rows; row++)
        if (row < 5 || row >= footer_row)
            memset(glyph_colors[row], colors[row], editor_screen_columns);

    size_t cursor_row = current_line - state->first_line + 5;
    if (((sys_ticks() / 25u) & 1u) &&
        cursor_row < footer_row &&
        screen_cursor_column + text_offset < editor_screen_columns)
        glyph_colors[cursor_row][screen_cursor_column + text_offset] = 0x0F;

    for (size_t row = 0; row < editor_screen_rows; row++) {
        if (screen_initialized &&
            colors[row] == previous_colors[row] &&
            strcmp(lines[row], previous_screen[row]) == 0 &&
            memcmp(glyph_colors[row], previous_glyph_colors[row],
                   editor_screen_columns) == 0)
            continue;
        size_t column = 0;
        while (column < editor_screen_columns) {
            uint8_t color = glyph_colors[row][column];
            size_t end = column + 1;
            char segment[EDITOR_MAX_COLUMNS + 1];
            while (end < editor_screen_columns &&
                   glyph_colors[row][end] == color)
                end++;
            memcpy(segment, lines[row] + column, end - column);
            segment[end - column] = 0;
            if (sys_set_cursor((int)column, row) >= 0)
                sys_puts(segment, color);
            column = end;
        }
    }
    /* Keep the hardware cursor aligned with the editor's visible caret. */
    if (cursor_row < footer_row)
        (void)sys_set_cursor((int)(screen_cursor_column + text_offset),
                             (int)cursor_row);
    memcpy(previous_screen, lines,
           editor_screen_rows * sizeof(previous_screen[0]));
    memcpy(previous_colors, colors, editor_screen_rows);
    memcpy(previous_glyph_colors, glyph_colors,
           editor_screen_rows * sizeof(previous_glyph_colors[0]));
    screen_initialized = 1;
}

static int load_text(editor_state_t* state, const char* path)
{
    fs_stat_t status;
    size_t normalized_length = 0;
    if (sys_stat(path, &status) < 0 || status.type != FS_TYPE_FILE ||
        status.size > EDITOR_CAPACITY)
        return -1;
    state->length = status.size;
    if (state->length) {
        ssize_t count = sys_read_at(path, state->data, state->length, 0);
        if (count != (ssize_t)state->length) return -1;
    }
    for (size_t i = 0; i < state->length; i++) {
        unsigned char ch = (unsigned char)state->data[i];
        if (ch == '\r') {
            if (i + 1 < state->length && state->data[i + 1] == '\n')
                continue;
            state->data[normalized_length++] = '\n';
            continue;
        }
        if (!ch || ch == 0x7F ||
            (ch < 32 && ch != '\n' && ch != '\t'))
            return -1;
        state->data[normalized_length++] = (char)ch;
    }
    state->length = normalized_length;
    state->data[state->length] = 0;
    return 0;
}

static void load_cursor_type(editor_state_t* state)
{
    char text[32];
    ssize_t count = sys_read_at("/etc/cursors/type", text, sizeof(text) - 1, 0);
    state->cursor_type = 0;
    if (count <= 0)
        return;
    text[count] = 0;
    while (count > 0 && (text[count - 1] == '\n' || text[count - 1] == '\r'))
        text[--count] = 0;
    if (strcmp(text, "underline") == 0 ||
        strcmp(text, "underscore") == 0 ||
        strcmp(text, "_") == 0 ||
        strcmp(text, "line") == 0)
        state->cursor_type = 1;
}

static int open_tab(const char* path)
{
    size_t length;
    editor_tab_t* tab;
    if (!path || !path[0])
        return -1;
    length = strlen(path);
    if (length >= EDITOR_PATH_MAX)
        return -1;
    for (int i = 0; i < editor_tab_count; i++) {
        if (strcmp(editor_tabs[i].path, path) == 0)
            return i;
    }
    if (editor_tab_count >= EDITOR_MAX_TABS)
        return -1;
    tab = &editor_tabs[editor_tab_count];
    memset(tab, 0, sizeof(*tab));
    if (load_text(&tab->editor, path) < 0)
        return -1;
    load_cursor_type(&tab->editor);
    memcpy(tab->path, path, length + 1);
    return editor_tab_count++;
}

static void tree_move_to_parent(void)
{
    char parent[EDITOR_PATH_MAX];
    size_t length = strlen(editor_tree_path);
    const char* slash = 0;
    if (length <= 1) return;
    for (size_t i = 0; i < length; i++)
        if (editor_tree_path[i] == '/') slash = editor_tree_path + i;
    if (!slash || slash == editor_tree_path) {
        strcpy(parent, "/");
    } else {
        size_t parent_length = (size_t)(slash - editor_tree_path);
        memcpy(parent, editor_tree_path, parent_length);
        parent[parent_length] = 0;
    }
    (void)tree_load_directory(parent);
}

static int tree_selected_path(char* path, size_t capacity)
{
    const char* name;
    size_t directory_length;
    size_t name_length;
    if (editor_tree_selected <= 0 ||
        editor_tree_selected > editor_tree_count)
        return -1;
    name = editor_tree_names[editor_tree_selected - 1];
    directory_length = strlen(editor_tree_path);
    name_length = strlen(name);
    if (directory_length + (directory_length > 1) + name_length >= capacity)
        return -1;
    memcpy(path, editor_tree_path, directory_length);
    if (directory_length > 1) path[directory_length++] = '/';
    memcpy(path + directory_length, name, name_length + 1);
    return 0;
}

static int tree_activate_selection(void)
{
    char path[EDITOR_PATH_MAX];
    fs_stat_t status;
    if (editor_tree_selected == 0) {
        tree_move_to_parent();
        return 0;
    }
    if (tree_selected_path(path, sizeof(path)) < 0 ||
        sys_stat(path, &status) < 0)
        return -1;
    if (status.type == FS_TYPE_DIR)
        return tree_load_directory(path);
    if (status.type != FS_TYPE_FILE)
        return -1;
    int tab = open_tab(path);
    if (tab < 0)
        return -1;
    editor_active_tab = tab;
    screen_initialized = 0;
    return 1;
}

static void* textedit_lua_allocator(void* context, void* pointer,
                                    size_t old_size, size_t new_size)
{
    void* replacement;
    (void)context;
    if (!new_size) {
        free(pointer);
        return 0;
    }
    replacement = malloc(new_size);
    if (!replacement) return 0;
    if (pointer) {
        size_t copy_size = old_size < new_size ? old_size : new_size;
        memcpy(replacement, pointer, copy_size);
        free(pointer);
    }
    return replacement;
}

static void textedit_lua_interrupt_hook(lua_State* state, lua_Debug* debug)
{
    (void)debug;
    if (sys_tty_take_ctrl_c())
        luaL_error(state, "interrupted");
}

static int suffix_equal(const char* path, const char* suffix)
{
    size_t length = strlen(path);
    size_t suffix_length = strlen(suffix);
    if (length < suffix_length)
        return 0;
    path += length - suffix_length;
    while (*suffix) {
        char left = *path++;
        char right = *suffix++;
        if (left >= 'A' && left <= 'Z')
            left = (char)(left - 'A' + 'a');
        if (right >= 'A' && right <= 'Z')
            right = (char)(right - 'A' + 'a');
        if (left != right)
            return 0;
    }
    return 1;
}

static int has_lua_suffix(const char* path)
{
    return suffix_equal(path, ".lua");
}

static int project_makefile_path(const char* path, char* makefile,
                                 size_t capacity)
{
    size_t directory_length = strlen(path);
    const char* slash = 0;
    fs_stat_t status;
    for (size_t i = 0; i < directory_length; i++)
        if (path[i] == '/')
            slash = path + i;
    if (!slash)
        return -1;
    directory_length = slash == path ? 1 : (size_t)(slash - path);
    if (directory_length + sizeof("/Makefile") > capacity)
        return -1;
    memcpy(makefile, path, directory_length);
    makefile[directory_length] = 0;
    if (strcmp(makefile, "/") != 0)
        strcat(makefile, "/");
    strcat(makefile, "Makefile");
    if (sys_stat(makefile, &status) == 0 && status.type == FS_TYPE_FILE)
        return 1;

    size_t makefile_length = strlen(makefile);
    if (makefile_length < sizeof("Makefile") - 1)
        return 0;
    memcpy(makefile + makefile_length - (sizeof("Makefile") - 1),
           "makefile", sizeof("makefile"));
    if (sys_stat(makefile, &status) == 0 && status.type == FS_TYPE_FILE)
        return 1;

    memcpy(makefile + makefile_length - (sizeof("makefile") - 1),
           "Makefile", sizeof("Makefile"));
    return 0;
}
__attribute__((unused))
static int generate_project_makefile(const char* source_path)
{
    char makefile[EDITOR_PATH_MAX];
    char project_name[USER_NAME_MAX];
    char contents[512];
    const char* source_name = path_basename(source_path);
    fs_stat_t status;
    int written;

    size_t source_name_length = strlen(source_name);
    if (source_name_length <= 2 || !suffix_equal(source_name, ".c"))
        return -1;
    source_name_length -= 2;
    if (source_name_length >= sizeof(project_name))
        return -1;
    memcpy(project_name, source_name, source_name_length);
    project_name[source_name_length] = 0;
    for (size_t i = 0; project_name[i]; i++) {
        char ch = project_name[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
            project_name[i] = '_';
    }
    if (!(project_name[0] >= 'A' && project_name[0] <= 'Z') &&
        !(project_name[0] >= 'a' && project_name[0] <= 'z') &&
        project_name[0] != '_') {
        size_t length = strlen(project_name);
        if (length + 4 >= sizeof(project_name))
            return -1;
        memmove(project_name + 4, project_name, length + 1);
        memcpy(project_name, "app_", 4);
    }
    for (const char* cursor = source_name; *cursor; cursor++)
        if (!((*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_' || *cursor == '.' || *cursor == '-'))
            return -1;

    int makefile_state = project_makefile_path(
        source_path, makefile, sizeof(makefile));
    if (makefile_state != 0)
        return makefile_state > 0 ? -2 : -1;
    if (sys_stat(source_path, &status) < 0 || status.type != FS_TYPE_FILE)
        return -1;
    written = snprintf(contents, sizeof(contents),
        "APP_NAME := %s\n"
        "APP_SRC := %s\n"
        "SDK_ROOT ?= /path/to/CreateOS/sdk\n"
        "include $(SDK_ROOT)/app.mk\n",
        project_name, source_name);
    if (written < 0 || (size_t)written >= sizeof(contents))
        return -1;

    int fd = sys_open(makefile, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0)
        return -1;
    size_t offset = 0;
    while (offset < (size_t)written) {
        ssize_t count = sys_write(fd, contents + offset,
                                  (size_t)written - offset);
        if (count <= 0 || (size_t)count > (size_t)written - offset) {
            (void)sys_close(fd);
            (void)sys_unlink(makefile);
            return -1;
        }
        offset += (size_t)count;
    }
    if (sys_close(fd) < 0 || sys_sync() < 0)
        return -1;
    return 0;
}

static int c_output_path(const char* source, char* output, size_t capacity)
{
    size_t length = strlen(source);
    if (length < 3 || length - 2 + 1 > capacity)
        return -1;
    memcpy(output, source, length - 2);
    output[length - 2] = 0;
    if (output[length - 3] == '/')
        return -1;
    return 0;
}

static void print_status_line(const char* label, int status, int color)
{
    char text[80];
    snprintf(text, sizeof(text), "%s (status %d)\n", label, status);
    sys_puts(text, color);
}

static void run_c_buffer(editor_tab_t* tab)
{
    char output[EDITOR_PATH_MAX];
    char* compile_argv[] = { TEXTEDIT_CC_PATH, tab->path, "-o", output, 0 };
    char* run_argv[] = { output, 0 };
    int result;

    if (c_output_path(tab->path, output, sizeof(output)) < 0) {
        tab->editor.message = "Cannot derive an output name from this path.";
        return;
    }
    if (tab->editor.dirty) {
        result = save_text(&tab->editor, tab->path);
        if (result < 0) {
            tab->editor.message = "Cannot save the file before compiling.";
            return;
        }
    }
    sys_clear();
    sys_puts("TextEdit: compiling with cc\n\n", 0x0B);
    result = sys_run(TEXTEDIT_CC_PATH, compile_argv);
    if (result != 0) {
        print_status_line("\nCompilation failed", result, 0x0C);
        tab->editor.message = "Compilation failed; see output above.";
    } else if (sys_chmod(output, 0755) < 0) {
        sys_puts("\nCannot mark the program executable.\n", 0x0C);
        tab->editor.message = "Compiled, but the program could not be made executable.";
    } else {
        sys_puts("Compiled. Running...\n\n", 0x0A);
        result = sys_run(output, run_argv);
        print_status_line("\nProgram exited", result,
                          result == 0 ? 0x0A : 0x0E);
        tab->editor.message = result == 0
            ? "Program finished."
            : "Program exited with a non-zero status.";
    }
    sys_puts("\n[Press any key to return to TextEdit]\n", 0x0E);
    (void)createos_ui_read_key();
    screen_initialized = 0;
}

static void run_current_buffer(editor_tab_t* tab)
{
    lua_State* state;
    int result;
    const char* message;
    if (suffix_equal(tab->path, ".py")) {
        tab->editor.message = "Python is not installed in this CreateOS build.";
        return;
    }
    if (suffix_equal(tab->path, ".c")) {
        int result = generate_project_makefile(tab->path);
        tab->editor.message = result == 0
            ? "Generated Makefile; guest make/cc are not installed."
            : result == -2
                ? "Makefile already exists; guest make/cc are not installed."
                : "Cannot generate Makefile; check project path and permissions.";
        return;
    }
    if (suffix_equal(tab->path, ".h") ||
        strcmp(path_basename(tab->path), "Makefile") == 0 ||
        strcmp(path_basename(tab->path), "makefile") == 0) {
        tab->editor.message =
            "Build is unavailable: guest make and cc are not installed.";
        return;
    }
    if (!has_lua_suffix(tab->path)) {
        tab->editor.message = "Run is available only for .lua files.";
        return;
    }
    state = lua_newstate(textedit_lua_allocator, 0);
    if (!state) {
        tab->editor.message = "Lua could not allocate an interpreter state.";
        return;
    }
    sys_clear();
    sys_puts("TextEdit: running current Lua buffer\n\n", 0x0B);
    luaL_openlibs(state);
    lua_getglobal(state, "package");
    if (lua_istable(state, -1)) {
        lua_pushliteral(state, "");
        lua_setfield(state, -2, "cpath");
    }
    lua_pop(state, 1);
    result = luaL_loadbufferx(state, tab->editor.data,
                              tab->editor.length, tab->path, 0);
    if (result == LUA_OK) {
        (void)sys_tty_ctrl_c_interrupt(1);
        lua_sethook(state, textedit_lua_interrupt_hook, LUA_MASKCOUNT, 10000);
        result = lua_pcall(state, 0, LUA_MULTRET, 0);
        lua_sethook(state, 0, 0, 0);
        (void)sys_tty_ctrl_c_interrupt(0);
    }
    if (result != LUA_OK) {
        message = lua_tostring(state, -1);
        sys_puts("Lua error: ", 0x0C);
        sys_puts(message ? message : "unknown interpreter error", 0x0C);
        sys_puts("\n", 0x0C);
    }
    lua_close(state);
    sys_puts("\n[Press any key to return to TextEdit]\n", 0x0E);
    (void)createos_ui_read_key();
    screen_initialized = 0;
    if (result == LUA_OK)
        tab->editor.message = "Lua script finished.";
    else
        tab->editor.message = "Lua script failed; see output above.";
}

static int prompt_open_path(char* path, size_t capacity)
{
    size_t length = 0;
    const char* prompt = "  Open file or folder path (Esc cancels): ";
    if (sys_set_cursor(0, (int)editor_screen_rows - 1) < 0)
        return -1;
    sys_puts(prompt, 0x0E);
    for (;;) {
        int key = createos_ui_read_key();
        if (key == 27)
            return 0;
        if (key == '\r' || key == '\n') {
            path[length] = 0;
            return length ? 1 : 0;
        }
        if (key == 8 || key == 127) {
            if (length) {
                length--;
                sys_puts("\b \b", 0x0F);
            }
            continue;
        }
        if (key < 32 || key > 126 || length + 1 >= capacity)
            continue;
        path[length++] = (char)key;
        sys_putchar((char)key);
    }
}

static int save_text(editor_state_t* state, const char* path)
{
    if (sys_file_replace(path, state->data, state->length) < 0) return -1;
    if (sys_sync() < 0) return -2;
    state->dirty = 0;
    state->message = "Saved.";
    return 0;
}

static int confirm_quit(void)
{
    char prompt[EDITOR_MAX_COLUMNS + 1];
    const char* prompt_text =
        "Unsaved tabs: (s)ave all, (d)iscard all, (c/Esc) cancel";
    int has_changes = 0;
    for (int i = 0; i < editor_tab_count; i++)
        if (editor_tabs[i].editor.dirty) has_changes = 1;
    if (!has_changes) {
        createos_ui_clear();
        return 1;
    }

    format_screen_line(prompt, prompt_text);
    if (sys_set_cursor(0, (int)editor_screen_rows - 1) < 0)
        return 0;
    sys_puts(prompt, 0x0E);
    (void)sys_set_cursor((int)strlen(prompt_text),
                         (int)editor_screen_rows - 1);

    for (;;) {
        int key = createos_ui_read_key();
        if (key == 'c' || key == 'C' || key == KEY_INTERRUPT ||
            key == KEY_CTRL_C || key == 27) {
            editor_tabs[editor_active_tab].editor.message = "Quit cancelled.";
            screen_initialized = 0;
            return 0;
        }
        if (key == 17 || key == 'd' || key == 'D' || key == 'q' || key == 'Q') {
            createos_ui_clear();
            return 1;
        }
        if (key == 's' || key == 'S') {
            for (int i = 0; i < editor_tab_count; i++) {
                editor_tab_t* tab = &editor_tabs[i];
                int result;
                if (!tab->editor.dirty) continue;
                result = save_text(&tab->editor, tab->path);
                if (result < 0) {
                    tab->editor.message = result == -2
                        ? "Written, but sync failed; tab remains open."
                        : "Save failed; tab remains open.";
                    editor_active_tab = i;
                    screen_initialized = 0;
                    return 0;
                }
            }
            createos_ui_clear();
            return 1;
        }
    }
}

static void update_viewport(editor_state_t* state)
{
    size_t cursor_line = line_number(state, state->cursor);
    if (cursor_line < state->first_line)
        state->first_line = cursor_line;
    else if (cursor_line >= state->first_line + editor_visible_lines)
        state->first_line =
            cursor_line - editor_visible_lines + 1;
}

static int place_cursor_from_mouse(editor_state_t* state, int x, int y)
{
    size_t footer_row = editor_screen_rows - 3;
    size_t target_line;
    size_t position = 0;
    size_t line = 0;
    size_t logical_column = 0;
    size_t target_column;
    if (y < 5 || (size_t)y >= footer_row || x < (int)editor_text_offset())
        return 0;
    x -= (int)editor_text_offset();

    target_line = state->first_line + (size_t)y - 5;
    while (position < state->length && line < target_line) {
        if (state->data[position++] == '\n') line++;
    }
    if (line < target_line) {
        state->cursor = state->length;
        return 1;
    }
    target_column = (size_t)x;
    if (target_column >= editor_visible_columns)
        target_column = editor_visible_columns - 1;
    while (position < state->length && state->data[position] != '\n') {
        size_t next_column = state->data[position] == '\t'
            ? logical_column + 4 - (logical_column % 4)
            : logical_column + 1;
        if (target_column < next_column) {
            if (state->data[position] == '\t' &&
                target_column - logical_column >=
                    (next_column - logical_column + 1) / 2)
                position++;
            state->cursor = position;
            return 1;
        }
        logical_column = next_column;
        position++;
    }
    state->cursor = position;
    return 1;
}

int main(int argc, char* argv[])
{
    createos_ui_t ui;
    char current_directory[EDITOR_PATH_MAX];
    char workspace_path[EDITOR_PATH_MAX];
    if (argc > EDITOR_MAX_TABS + 1) {
        sys_puts("Usage: textedit [folder | file [additional-files...]]\n",
                 0x0E);
        return 2;
    }
    if (sys_getcwd(current_directory, sizeof(current_directory)) < 0) {
        sys_puts("textedit: cannot determine the current folder\n", 0x0C);
        return 1;
    }
    memset(editor_tabs, 0, sizeof(editor_tabs));
    memset(&workspace_tab, 0, sizeof(workspace_tab));
    editor_tab_count = 0;
    editor_active_tab = 0;
    if (argc == 1) {
        strcpy(workspace_path, current_directory);
        if (tree_initialize_directory(workspace_path) < 0) {
            sys_puts("textedit: cannot open the current folder\n", 0x0C);
            return 1;
        }
    } else {
        char first_path[EDITOR_PATH_MAX];
        fs_stat_t first_status;
        if (resolve_path(argv[1], current_directory, first_path,
                         sizeof(first_path)) < 0 ||
            sys_stat(first_path, &first_status) < 0) {
            printf("textedit: path not found: %s\n", argv[1]);
            return 1;
        }
        int first_file = 1;
        if (first_status.type == FS_TYPE_DIR) {
            strcpy(workspace_path, first_path);
            if (tree_initialize_directory(workspace_path) < 0) {
                printf("textedit: cannot open folder: %s\n", first_path);
                return 1;
            }
            first_file = 2;
        } else {
            if (first_status.type != FS_TYPE_FILE) {
                printf("textedit: not a file or folder: %s\n", first_path);
                return 1;
            }
            tree_initialize(first_path);
            strcpy(workspace_path, editor_tree_path);
        }
        for (int i = first_file; i < argc; i++) {
            char file_path[EDITOR_PATH_MAX];
            fs_stat_t status;
            if (resolve_path(argv[i], workspace_path, file_path,
                             sizeof(file_path)) < 0 ||
                sys_stat(file_path, &status) < 0 ||
                status.type != FS_TYPE_FILE || open_tab(file_path) < 0) {
                printf("textedit: cannot open text file (limit %u bytes): %s\n",
                       EDITOR_CAPACITY, argv[i]);
                return 1;
            }
        }
    }

    strcpy(workspace_tab.path, "No file selected");
    workspace_tab.editor.message = "Choose a file in the tree or press Ctrl+O.";
    editor_detect_screen_size();
    screen_initialized = 0;
    if (createos_ui_init(&ui) < 0)
        return 1;
    (void)sys_tty_ctrl_c_interrupt(0);

    int mouse_selecting = 0;
    for (;;) {
        int key;
        editor_tab_t* tab = editor_tab_count
            ? &editor_tabs[editor_active_tab] : &workspace_tab;
        editor_state_t* state = &tab->editor;
        unsigned long drawn_cursor_phase = sys_ticks() / 25u;
        tree_keep_selection_visible();
        draw_editor(state, tab->path);
        state->message = 0;
        key = sys_tty_pollchar();
        while (!key) {
            unsigned long cursor_phase = sys_ticks() / 25u;
            if (cursor_phase != drawn_cursor_phase) {
                draw_editor(state, tab->path);
                drawn_cursor_phase = cursor_phase;
            }
            (void)sys_sleep(20);
            key = sys_tty_pollchar();
        }
        if (!editor_tab_count) {
            if (key == 2) {
                editor_tree_visible = !editor_tree_visible;
                editor_tree_focused = editor_tree_visible;
                update_editor_visible_columns();
            } else if (key == 15) {
                char path[EDITOR_PATH_MAX];
                int entered = prompt_open_path(path, sizeof(path));
                screen_initialized = 0;
                if (entered > 0) {
                    char resolved[EDITOR_PATH_MAX];
                    fs_stat_t status;
                    if (resolve_path(path, editor_tree_path, resolved,
                                     sizeof(resolved)) < 0 ||
                        sys_stat(resolved, &status) < 0) {
                        state->message = "Path not found.";
                    } else if (status.type == FS_TYPE_DIR) {
                        if (tree_load_directory(resolved) < 0)
                            state->message = "Cannot open folder.";
                    } else if (status.type == FS_TYPE_FILE) {
                        int next = open_tab(resolved);
                        if (next < 0)
                            state->message = "Cannot open text file.";
                        else
                            editor_active_tab = next;
                    } else {
                        state->message = "Path is not a file or folder.";
                    }
                }
            } else if (editor_tree_focused &&
                       (key == KEY_UP || key == KEY_SCROLL_UP)) {
                if (editor_tree_selected > 0)
                    editor_tree_selected--;
                tree_keep_selection_visible();
            } else if (editor_tree_focused &&
                       (key == KEY_DOWN || key == KEY_SCROLL_DOWN)) {
                if (editor_tree_selected < editor_tree_count)
                    editor_tree_selected++;
                tree_keep_selection_visible();
            } else if (editor_tree_focused &&
                       (key == '\r' || key == '\n')) {
                int result = tree_activate_selection();
                if (result < 0)
                    state->message = "Cannot open selected tree item.";
                else if (result == 0)
                    state->message = "Opened folder.";
            } else if (key == 17 || key == KEY_INTERRUPT || key == 27) {
                if (confirm_quit()) {
                    (void)sys_tty_pointer_mode(0);
                    (void)sys_tty_ctrl_c_interrupt(1);
                    (void)createos_ui_shutdown();
                    return 0;
                }
            } else if (key == KEY_MOUSE_LEFT) {
                int position = createos_ui_mouse_position();
                int x = position & 0xFFFF;
                int y = (int)(((unsigned int)position >> 16) & 0xFFFF);
                if (editor_tree_visible && x < EDITOR_TREE_WIDTH &&
                    y >= 5 && (size_t)y < editor_screen_rows - 3) {
                    int selected = editor_tree_first + y - 5;
                    if (selected <= editor_tree_count) {
                        editor_tree_selected = selected;
                        editor_tree_focused = 1;
                        tree_keep_selection_visible();
                    }
                }
            } else if (key == 18) {
                state->message = "Open a .lua file before running.";
            } else {
                state->message = "Open a file from the tree or Ctrl+O.";
            }
            screen_initialized = 0;
            continue;
        }
        if (key == 2) {
            editor_tree_visible = !editor_tree_visible;
            editor_tree_focused = editor_tree_visible;
            update_editor_visible_columns();
            screen_initialized = 0;
        } else if (key == 18) {
            if (editor_tab_count)
                run_current_buffer(tab);
            else
                state->message = "Open a file from the tree first.";
        } else if (editor_tree_focused &&
                   (key == KEY_UP || key == KEY_SCROLL_UP)) {
            if (editor_tree_selected > 0) editor_tree_selected--;
            tree_keep_selection_visible();
            screen_initialized = 0;
        } else if (editor_tree_focused &&
                   (key == KEY_DOWN || key == KEY_SCROLL_DOWN)) {
            if (editor_tree_selected < editor_tree_count)
                editor_tree_selected++;
            tree_keep_selection_visible();
            screen_initialized = 0;
        } else if (editor_tree_focused &&
                   (key == '\r' || key == '\n')) {
            int result = tree_activate_selection();
            if (result < 0)
                state->message = "Cannot open selected tree item.";
            else if (result == 0)
                state->message = "Opened folder.";
            screen_initialized = 0;
        } else if (key == 19) {
            int result = save_text(state, tab->path);
            state->message = result == 0 ? "Saved."
                : result == -2 ? "Save accepted, but sync failed."
                               : "Save failed; check file permissions.";
        } else if (key == 15) {
            char path[EDITOR_PATH_MAX];
            int entered = prompt_open_path(path, sizeof(path));
            screen_initialized = 0;
            if (entered > 0) {
                char resolved[EDITOR_PATH_MAX];
                fs_stat_t status;
                if (resolve_path(path, editor_tree_path, resolved,
                                 sizeof(resolved)) < 0 ||
                    sys_stat(resolved, &status) < 0) {
                    state->message = "Path not found.";
                    continue;
                }
                if (status.type == FS_TYPE_DIR) {
                    if (tree_load_directory(resolved) < 0)
                        state->message = "Cannot open folder.";
                    continue;
                }
                int next = status.type == FS_TYPE_FILE
                    ? open_tab(resolved) : -1;
                if (next < 0)
                    state->message = editor_tab_count >= EDITOR_MAX_TABS
                        ? "Tab limit reached (8 open files)."
                        : "Cannot open text file (missing, binary, or too large).";
                else {
                    editor_active_tab = next;
                    screen_initialized = 0;
                }
            }
        } else if (key == KEY_CTRL_TAB) {
            editor_active_tab = (editor_active_tab + 1) % editor_tab_count;
            screen_initialized = 0;
        } else if (key == KEY_CTRL_C) {
            state->message = copy_selection(state)
                ? "Copied selection." : "Select text to copy.";
        } else if (key == KEY_CTRL_X) {
            if (!copy_selection(state))
                state->message = "Select text to cut.";
            else
                (void)delete_selection(state);
        } else if (key == KEY_CTRL_V) {
            int clipboard_length =
                sys_clipboard_get(editor_clipboard, sizeof(editor_clipboard));
            if (clipboard_length < 0)
                state->message = "Could not read the system clipboard.";
            else if (clipboard_length &&
                !replace_selection(state, editor_clipboard,
                                   (size_t)clipboard_length))
                state->message = "Paste exceeds the 4096-byte file limit.";
        } else if (key == 17 || key == KEY_INTERRUPT || key == 27) {
            if (confirm_quit()) {
                (void)sys_tty_pointer_mode(0);
                (void)sys_tty_ctrl_c_interrupt(1);
                (void)createos_ui_shutdown();
                return 0;
            }
        } else if (key == KEY_LEFT) {
            if (state->cursor) set_cursor(state, state->cursor - 1, 0);
        } else if (key == KEY_RIGHT) {
            if (state->cursor < state->length)
                set_cursor(state, state->cursor + 1, 0);
        } else if (key == KEY_UP) {
            state->selection_active = 0;
            move_vertical(state, -1);
        } else if (key == KEY_DOWN) {
            state->selection_active = 0;
            move_vertical(state, 1);
        } else if (key == KEY_SHIFT_LEFT || key == KEY_SHIFT_RIGHT) {
            size_t next = state->cursor;
            if (key == KEY_SHIFT_LEFT && next) next--;
            if (key == KEY_SHIFT_RIGHT && next < state->length) next++;
            set_cursor(state, next, 1);
        } else if (key == KEY_SHIFT_UP || key == KEY_SHIFT_DOWN) {
            if (!state->selection_active) {
                state->selection_anchor = state->cursor;
                state->selection_active = 1;
            }
            move_vertical(state, key == KEY_SHIFT_UP ? -1 : 1);
        } else if (key == KEY_SHIFT_HOME || key == KEY_SHIFT_END) {
            size_t next = key == KEY_SHIFT_HOME
                ? line_start(state, state->cursor)
                : line_end(state, state->cursor);
            set_cursor(state, next, 1);
        } else if (key == KEY_MOUSE_LEFT) {
            int position = createos_ui_mouse_position();
            int x = position & 0xFFFF;
            int y = (int)(((unsigned int)position >> 16) & 0xFFFF);
            update_pointer_mode(
                y);
            if (editor_tree_visible && x < EDITOR_TREE_WIDTH &&
                y >= 5 && (size_t)y < editor_screen_rows - 3) {
                int selected = editor_tree_first + y - 5;
                if (selected <= editor_tree_count) {
                    editor_tree_selected = selected;
                    editor_tree_focused = 1;
                    tree_keep_selection_visible();
                    screen_initialized = 0;
                }
                mouse_selecting = 0;
            } else if (place_cursor_from_mouse(state, x, y)) {
                editor_tree_focused = 0;
                state->selection_anchor = state->cursor;
                state->selection_active = 1;
                mouse_selecting = 1;
            } else {
                mouse_selecting = 0;
            }
        } else if (key == KEY_MOUSE_DRAG && mouse_selecting) {
            int position = createos_ui_mouse_position();
            update_pointer_mode(
                (int)(((unsigned int)position >> 16) & 0xFFFF));
            (void)place_cursor_from_mouse(
                state, position & 0xFFFF,
                (int)(((unsigned int)position >> 16) & 0xFFFF));
        } else if (key == KEY_MOUSE_RELEASE) {
            int position = createos_ui_mouse_position();
            update_pointer_mode(
                (int)(((unsigned int)position >> 16) & 0xFFFF));
            if (mouse_selecting) {
                (void)place_cursor_from_mouse(
                    state, position & 0xFFFF,
                    (int)(((unsigned int)position >> 16) & 0xFFFF));
            }
            mouse_selecting = 0;
        } else if (key == KEY_MOUSE_MOVE) {
            int position = createos_ui_mouse_position();
            update_pointer_mode(
                (int)(((unsigned int)position >> 16) & 0xFFFF));
        } else if (key == KEY_DELETE) {
            if (!delete_selection(state) && state->cursor < state->length)
                erase_bytes(state, state->cursor, 1);
        } else if (key == 8 || key == 127) {
            if (!delete_selection(state) && state->cursor) {
                state->cursor--;
                erase_bytes(state, state->cursor, 1);
            }
        } else if (key == 1) {
            set_cursor(state, line_start(state, state->cursor), 0);
        } else if (key == 5) {
            set_cursor(state, line_end(state, state->cursor), 0);
        } else if (key == '\r' || key == '\n') {
            const char newline = '\n';
            if (!replace_selection(state, &newline, 1))
                state->message = "File limit reached (4096 bytes).";
        } else if (key == KEY_TAB || key == '\t') {
            const char tab = '\t';
            if (!replace_selection(state, &tab, 1))
                state->message = "File limit reached (4096 bytes).";
        } else if ((key >= 32 && key <= 126) || key >= 128) {
            char character = (char)key;
            if (!replace_selection(state, &character, 1))
                state->message = "File limit reached (4096 bytes).";
        }
        update_viewport(state);
    }
}