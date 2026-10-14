// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "user_syscall.h"
#include "stdio.h"

#define LS_MAX_ENTRIES 128
#define LS_MAX_DEPTH 8

typedef struct {
    char names[LS_MAX_ENTRIES][USER_NAME_MAX];
    fs_stat_t statuses[LS_MAX_ENTRIES];
    uint8_t have_status[LS_MAX_ENTRIES];
} listing_context_t;

static listing_context_t listing_contexts[LS_MAX_DEPTH + 1];
static int show_all;
static int long_format;
static int human_sizes;
static int recursive;
static int classify;
static int directories_only;
static int one_per_line;
static int use_colors = 1;

static int child_path(const char* parent, const char* name, char* output,
                      size_t output_size)
{
    size_t parent_length = strlen(parent);
    size_t name_length = strlen(name);
    int separator = parent_length > 0 && parent[parent_length - 1] != '/';
    size_t required = parent_length + (size_t)separator + name_length + 1;

    if (required > output_size) return 0;
    memcpy(output, parent, parent_length);
    if (separator) output[parent_length++] = '/';
    memcpy(output + parent_length, name, name_length + 1);
    return 1;
}

static int entry_visible(const char* name)
{
    if (show_all) return 1;
    return name[0] != '.';
}

static void print_unsigned(uint32_t value)
{
    char digits[10];
    size_t length = 0;
    do {
        digits[length++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (length) sys_putchar(digits[--length]);
}

static void print_size(uint32_t size)
{
    static const char units[] = "BKMGT";
    uint32_t whole = size;
    uint32_t unit = 0;

    if (!human_sizes) {
        print_unsigned(size);
        return;
    }
    while (whole >= 1024 && unit < sizeof(units) - 2) {
        whole /= 1024;
        unit++;
    }
    print_unsigned(whole);
    sys_putchar(units[unit]);
}

static void print_mode(const fs_stat_t* status)
{
    static const uint16_t bits[] = { 0400, 0200, 0100, 0040, 0020,
                                     0010, 0004, 0002, 0001 };
    static const char letters[] = "rwxrwxrwx";
    sys_putchar(status->type == CFS5_INODE_DIR ? 'd' : '-');
    for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
        sys_putchar((status->mode & bits[i]) ? letters[i] : '-');
}

static int entry_color(const fs_stat_t* status)
{
    if (status->type == CFS5_INODE_DIR) return 0x0B;
    if (status->mode & 0111) return 0x0A;
    return 0x0F;
}

static void print_name(const char* name, const fs_stat_t* status,
                       int have_status)
{
    if (use_colors && have_status)
        sys_puts(name, entry_color(status));
    else
        sys_puts(name, 0x0F);
    if (classify && have_status) {
        if (status->type == CFS5_INODE_DIR) sys_putchar('/');
        else if (status->mode & 0111) sys_putchar('*');
    }
}

static int list_path(const char* path, int depth)
{
    fs_stat_t directory;
    listing_context_t* listing = &listing_contexts[depth];
    char (*names)[USER_NAME_MAX] = listing->names;
    fs_stat_t* statuses = listing->statuses;
    uint8_t* have_status = listing->have_status;
    int result = 0;
    memset(have_status, 0, sizeof(listing->have_status));

    if (sys_stat(path, &directory) < 0) {
        sys_puts("ls: cannot access: ", 0x0C);
        sys_puts(path, 0x0C);
        sys_putchar('\n');
        return 1;
    }
    if (directory.type != CFS5_INODE_DIR) {
        if (long_format) {
            print_mode(&directory);
            sys_putchar(' ');
            printf("%d %d ", (int)directory.uid, (int)directory.gid);
            print_size(directory.size);
            sys_putchar(' ');
        }
        print_name(path, &directory, 1);
        sys_putchar('\n');
        return 0;
    }
    if (directories_only) {
        if (long_format) {
            print_mode(&directory);
            sys_putchar(' ');
            printf("%d %d ", (int)directory.uid, (int)directory.gid);
            print_size(directory.size);
            sys_putchar(' ');
        }
        print_name(path, &directory, 1);
        sys_putchar('\n');
        return 0;
    }

    int count = sys_list(path, names, LS_MAX_ENTRIES);
    if (count < 0) {
        sys_puts("ls: cannot list directory: ", 0x0C);
        sys_puts(path, 0x0C);
        sys_putchar('\n');
        return 1;
    }
    if (count == LS_MAX_ENTRIES) {
        sys_puts("ls: listing reached 128 entries; additional entries may be omitted\n",
                 0x0E);
    }

    for (int i = 0; i < count; i++) {
        char child[256];
        if (!entry_visible(names[i])) continue;
        if (!child_path(path, names[i], child, sizeof(child))) {
            sys_puts("ls: path is too long: ", 0x0C);
            sys_puts(names[i], 0x0C);
            sys_putchar('\n');
            result = 1;
            continue;
        }
        if (sys_stat(child, &statuses[i]) == 0) {
            have_status[i] = 1;
        } else {
            sys_puts("ls: cannot stat: ", 0x0C);
            sys_puts(child, 0x0C);
            sys_putchar('\n');
            result = 1;
        }
    }

    if (long_format) {
        for (int i = 0; i < count; i++) {
            if (!entry_visible(names[i])) continue;
            if (have_status[i]) {
                print_mode(&statuses[i]);
                sys_putchar(' ');
                printf("%d %d ", (int)statuses[i].uid,
                       (int)statuses[i].gid);
                print_size(statuses[i].size);
                sys_putchar(' ');
                print_name(names[i], &statuses[i], 1);
            } else {
                sys_puts("????????? ? ? ? ", 0x0C);
                print_name(names[i], NULL, 0);
            }
            sys_putchar('\n');
        }
    } else {
        int column = 0;
        for (int i = 0; i < count; i++) {
            if (!entry_visible(names[i])) continue;
            print_name(names[i], &statuses[i], have_status[i]);
            if (one_per_line) {
                sys_putchar('\n');
            } else {
                sys_puts("  ", 0x07);
                if (++column == 4) {
                    sys_putchar('\n');
                    column = 0;
                }
            }
        }
        if (column) sys_putchar('\n');
    }

    if (recursive) {
        for (int i = 0; i < count; i++) {
            char child[256];
            if (!entry_visible(names[i]) || !have_status[i] ||
                statuses[i].type != CFS5_INODE_DIR ||
                strcmp(names[i], ".") == 0 ||
                strcmp(names[i], "..") == 0)
                continue;
            if (!child_path(path, names[i], child, sizeof(child))) {
                result = 1;
                continue;
            }
            if (depth >= LS_MAX_DEPTH) {
                sys_puts("ls: recursion depth limit reached at ", 0x0C);
                sys_puts(child, 0x0C);
                sys_putchar('\n');
                result = 1;
                continue;
            }
            sys_putchar('\n');
            sys_puts(child, 0x0E);
            sys_puts(":\n", 0x0E);
            if (list_path(child, depth + 1)) result = 1;
        }
    }
    return result;
}

static int parse_option(const char* option)
{
    if (strcmp(option, "--help") == 0) {
        sys_puts("Usage: ls [-lahR1dF] [--color=always|never] [path ...]\n",
                 0x0F);
        sys_puts("  -l long listing  -a include hidden  -h human sizes\n", 0x0F);
        sys_puts("  -R recursive     -r recursive (legacy) -1 one entry per line\n",
                 0x0F);
        sys_puts("  -d list directory itself  -F append / to directories, * to executables\n",
                 0x0F);
        return 2;
    }
    if (strcmp(option, "--all") == 0) { show_all = 1; return 1; }
    if (strcmp(option, "--long") == 0) { long_format = 1; return 1; }
    if (strcmp(option, "--human-readable") == 0) {
        human_sizes = 1;
        return 1;
    }
    if (strcmp(option, "--recursive") == 0) { recursive = 1; return 1; }
    if (strcmp(option, "--classify") == 0) { classify = 1; return 1; }
    if (strcmp(option, "--directory") == 0) {
        directories_only = 1;
        return 1;
    }
    if (strcmp(option, "--one-per-line") == 0) {
        one_per_line = 1;
        return 1;
    }
    if (strcmp(option, "--color=never") == 0) {
        use_colors = 0;
        return 1;
    }
    if (strcmp(option, "--color=always") == 0) {
        use_colors = 1;
        return 1;
    }
    if (option[0] != '-' || !option[1]) return 0;
    for (size_t i = 1; option[i]; i++) {
        switch (option[i]) {
        case 'a': show_all = 1; break;
        case 'l': long_format = 1; break;
        case 'h': human_sizes = 1; break;
        case 'r':
        case 'R': recursive = 1; break;
        case 'F': classify = 1; break;
        case 'd': directories_only = 1; break;
        case '1': one_per_line = 1; break;
        default:
            sys_puts("ls: unknown option: ", 0x0C);
            sys_puts(option, 0x0C);
            sys_putchar('\n');
            return -1;
        }
    }
    return 1;
}

int main(int argc, char* argv[])
{
    char cwd[256];
    char* paths[32];
    int path_count = 0;
    int options = 1;

    if (sys_getcwd(cwd, sizeof(cwd)) < 0) strcpy(cwd, "/");
    for (int i = 1; i < argc; i++) {
        if (options && strcmp(argv[i], "--") == 0) {
            options = 0;
            continue;
        }
        if (options && argv[i][0] == '-') {
            int parsed = parse_option(argv[i]);
            if (parsed == 2) return 0;
            if (parsed < 0) return 2;
            if (parsed > 0) continue;
        }
        if (path_count == (int)(sizeof(paths) / sizeof(paths[0]))) {
            sys_puts("ls: too many paths\n", 0x0C);
            return 2;
        }
        paths[path_count++] = argv[i];
    }
    if (!path_count) paths[path_count++] = cwd;

    int result = 0;
    for (int i = 0; i < path_count; i++) {
        if (path_count > 1) {
            sys_puts(paths[i], 0x0E);
            sys_puts(":\n", 0x0E);
        }
        if (list_path(paths[i], 0)) result = 1;
        if (i + 1 < path_count) sys_putchar('\n');
    }
    return result;
}

void _start(void)
{
    extern int main(int argc, char* argv[]);
    sys_puts("ls: requires C runtime for argc/argv\n", 0x0C);
    sys_exit(0);
}
