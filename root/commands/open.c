#include "user_syscall.h"

#define APP_LIST_MAX 64
#define APP_PATH_MAX 256
#define LINK_TARGET_MAX 256

static char applications[APP_LIST_MAX][USER_NAME_MAX];
static char application_paths[APP_LIST_MAX][APP_PATH_MAX];
static int application_count;

static int has_suffix(const char* name, const char* suffix)
{
    size_t name_length = strlen(name);
    size_t suffix_length = strlen(suffix);
    return name_length >= suffix_length &&
           strcmp(name + name_length - suffix_length, suffix) == 0;
}

static int normalize_app_name(const char* name, char* output, size_t capacity)
{
    size_t length;
    if (!name || !output ||
        (!has_suffix(name, ".APPL") && !has_suffix(name, ".appl")))
        return 0;
    length = strlen(name);
    if (length >= capacity) return 0;
    memcpy(output, name, length + 1);
    output[length - 4] = 'A';
    output[length - 3] = 'P';
    output[length - 2] = 'P';
    output[length - 1] = 'L';
    return 1;
}

static int valid_app_target(const char* target)
{
    const char* component;
    if (strncmp(target, "/app/", 5) != 0 ||
        (!has_suffix(target, ".APPL") && !has_suffix(target, ".appl")))
        return 0;
    component = target + 5;
    while (*component) {
        const char* end = strchr(component, '/');
        size_t length = end ? (size_t)(end - component) : strlen(component);
        if (!length || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.'))
            return 0;
        if (!end) break;
        component = end + 1;
    }
    return 1;
}

static int load_link_file(const char* link_path, char* app_path)
{
    char target[LINK_TARGET_MAX];
    fs_stat_t status;
    size_t target_length;

    if (sys_stat(link_path, &status) < 0 || status.type != FS_TYPE_FILE ||
        status.size == 0 || status.size >= sizeof(target))
        return 0;

    ssize_t count = sys_read_at(link_path, target, status.size, 0);
    if (count != (ssize_t)status.size) return 0;
    target_length = status.size;
    while (target_length &&
           (target[target_length - 1] == '\n' ||
            target[target_length - 1] == '\r'))
        target_length--;
    if (!target_length || target_length >= sizeof(target)) return 0;
    target[target_length] = 0;

    if (!valid_app_target(target)) return 0;
    if (!normalize_app_name(target, app_path, APP_PATH_MAX)) return 0;
    if (sys_stat(app_path, &status) < 0 || status.type != FS_TYPE_FILE)
        return 0;
    return 1;
}

static int current_user_home(char* home, size_t capacity)
{
    user_t user;
    size_t length;
    if (sys_user_current(&user) != 1 ||
        strncmp(user.home, "/home/", 6) != 0)
        return 0;
    length = strlen(user.home);
    if (length >= capacity) return 0;
    memcpy(home, user.home, length + 1);
    return 1;
}

static int app_name_equal(const char* left, const char* right)
{
    while (*left && *right) {
        char left_char = *left++;
        char right_char = *right++;
        if (left_char >= 'A' && left_char <= 'Z')
            left_char = (char)(left_char - 'A' + 'a');
        if (right_char >= 'A' && right_char <= 'Z')
            right_char = (char)(right_char - 'A' + 'a');
        if (left_char != right_char) return 0;
    }
    return *left == *right;
}

static int load_system_link(const char* app_name, char* app_path)
{
    char link_path[APP_PATH_MAX];
    size_t name_length = strlen(app_name);
    if (name_length <= 5 ||
        12 + name_length - 5 + 6 > sizeof(link_path))
        return 0;
    memcpy(link_path, "/app/system/", 12);
    memcpy(link_path + 12, app_name, name_length - 5);
    memcpy(link_path + 12 + name_length - 5, ".link", 6);
    return load_link_file(link_path, app_path);
}

static int load_user_link(const char* app_name, char* app_path)
{
    char home[64];
    char directory[APP_PATH_MAX];
    char link_path[APP_PATH_MAX];
    char names[APP_LIST_MAX][USER_NAME_MAX];
    int count;

    if (strchr(app_name, '/') || !current_user_home(home, sizeof(home)) ||
        snprintf(directory, sizeof(directory), "%s/Applications", home) >=
            (int)sizeof(directory) ||
        snprintf(link_path, sizeof(link_path), "%s/%s", directory, app_name) >=
            (int)sizeof(link_path))
        return 0;
    if (load_link_file(link_path, app_path)) return 1;

    count = sys_list(directory, names, APP_LIST_MAX);
    if (count < 0) return 0;
    if (count > APP_LIST_MAX) count = APP_LIST_MAX;
    for (int i = 0; i < count; i++) {
        if ((has_suffix(names[i], ".APPL") ||
             has_suffix(names[i], ".appl")) &&
            app_name_equal(names[i], app_name)) {
            if (snprintf(link_path, sizeof(link_path), "%s/%s", directory,
                         names[i]) >= (int)sizeof(link_path))
                return 0;
            return load_link_file(link_path, app_path);
        }
    }
    return 0;
}

static int resolve_application(const char* name, char* app_path)
{
    char normalized[APP_PATH_MAX];
    char home[64];
    fs_stat_t status;
    const char* basename;

    if (!normalize_app_name(name, normalized, sizeof(normalized))) {
        sys_puts("open: expected an .APPL application name\n", 0x0C);
        return 0;
    }
    if (name[0] != '/' &&
        (sys_stat(name, &status) == 0 && status.type == FS_TYPE_FILE)) {
        size_t length = strlen(name);
        if (length >= APP_PATH_MAX) return 0;
        memcpy(app_path, name, length + 1);
        return 1;
    }
    if (strchr(name, '/')) {
        if (strncmp(name, "/app/", 5) == 0) {
            basename = name + 5;
            if (!basename[0] || strchr(basename, '/')) {
                sys_puts("open: application paths must name a file directly in /app\n",
                         0x0C);
                return 0;
            }
            if (snprintf(app_path, APP_PATH_MAX, "/app/%s", normalized + 5) >=
                APP_PATH_MAX)
                return 0;
            if (sys_stat(app_path, &status) < 0 ||
                status.type != FS_TYPE_FILE) {
                printf("open: application not found: %s\n", app_path);
                return 0;
            }
            return 1;
        }
        if (!current_user_home(home, sizeof(home)) ||
            snprintf(app_path, APP_PATH_MAX, "%s/Applications/", home) >=
                APP_PATH_MAX ||
            strncmp(name, app_path, strlen(app_path)) != 0) {
            sys_puts("open: application path must be in /app or your Applications folder\n",
                     0x0C);
            return 0;
        }
        basename = name + strlen(app_path);
        if (!basename[0] || strchr(basename, '/')) {
            sys_puts("open: application paths must name a shortcut in Applications\n",
                     0x0C);
            return 0;
        }
        if (!normalize_app_name(basename, normalized, sizeof(normalized)) ||
            snprintf(app_path, APP_PATH_MAX, "%s/Applications/%s", home,
                     normalized) >= APP_PATH_MAX)
            return 0;
        if (!load_link_file(app_path, normalized)) {
            printf("open: application shortcut not found: %s\n", app_path);
            return 0;
        }
        strcpy(app_path, normalized);
        return 1;
    }

    if (load_user_link(normalized, app_path)) return 1;
    if (snprintf(app_path, APP_PATH_MAX, "/app/%s", normalized) >=
        APP_PATH_MAX)
        return 0;
    if (sys_stat(app_path, &status) < 0 || status.type != FS_TYPE_FILE) {
        if (load_system_link(normalized, app_path)) return 1;
        printf("open: application not found: %s\n", normalized);
        return 0;
    }
    return 1;
}

static void add_application(const char* name, const char* path)
{
    size_t length = strlen(name);
    if (!path || length >= USER_NAME_MAX || application_count >= APP_LIST_MAX)
        return;
    for (int i = 0; i < application_count; i++)
        if (app_name_equal(applications[i], name) ||
            app_name_equal(application_paths[i], path))
            return;
    memcpy(applications[application_count], name, length + 1);
    strcpy(application_paths[application_count], path);
    application_count++;
}

static void sort_applications(void)
{
    for (int i = 1; i < application_count; i++) {
        char current[USER_NAME_MAX];
        strcpy(current, applications[i]);
        int j = i;
        while (j > 0 && strcmp(applications[j - 1], current) > 0) {
            strcpy(applications[j], applications[j - 1]);
            j--;
        }
        strcpy(applications[j], current);
    }
}

static int list_applications(void)
{
    char names[APP_LIST_MAX][USER_NAME_MAX];
    char home[64];
    char user_apps[APP_PATH_MAX];
    int count;
    fs_stat_t status;

    application_count = 0;
    if (current_user_home(home, sizeof(home)) &&
        snprintf(user_apps, sizeof(user_apps), "%s/Applications", home) <
            (int)sizeof(user_apps)) {
        count = sys_list(user_apps, names, APP_LIST_MAX);
        if (count < 0) {
            sys_puts("open: cannot list user Applications directory\n", 0x0C);
            return 1;
        }
        if (count > APP_LIST_MAX) count = APP_LIST_MAX;
        for (int i = 0; i < count; i++) {
            char resolved[APP_PATH_MAX];
            if ((has_suffix(names[i], ".APPL") ||
                 has_suffix(names[i], ".appl")) &&
                load_user_link(names[i], resolved))
                add_application(names[i], resolved);
        }
    }

    count = sys_list("/app", names, APP_LIST_MAX);
    if (count < 0) {
        sys_puts("open: cannot list /app\n", 0x0C);
        return 1;
    }
    if (count > APP_LIST_MAX) count = APP_LIST_MAX;
    for (int i = 0; i < count; i++) {
        if (!has_suffix(names[i], ".APPL") &&
            !has_suffix(names[i], ".appl"))
            continue;
        char path[APP_PATH_MAX];
        if (snprintf(path, sizeof(path), "/app/%s", names[i]) >=
            (int)sizeof(path) ||
            sys_stat(path, &status) < 0 || status.type != FS_TYPE_FILE)
            continue;
        add_application(names[i], path);
    }

    count = sys_list("/app/system", names, APP_LIST_MAX);
    if (count < 0) {
        sys_puts("open: cannot list /app/system shortcuts\n", 0x0C);
        return 1;
    }
    if (count > APP_LIST_MAX) count = APP_LIST_MAX;
    for (int i = 0; i < count; i++) {
        size_t length = strlen(names[i]);
        char display_name[USER_NAME_MAX];
        char resolved[APP_PATH_MAX];
        if (!has_suffix(names[i], ".link") || length - 5 + 6 >
            sizeof(display_name))
            continue;
        memcpy(display_name, names[i], length - 5);
        memcpy(display_name + length - 5, ".APPL", 6);
        if (load_system_link(display_name, resolved))
            add_application(display_name, resolved);
    }

    sort_applications();
    sys_puts("Applications:\n", 0x0B);
    if (!application_count) {
        sys_puts("  (none)\n", 0x07);
        return 0;
    }
    for (int i = 0; i < application_count; i++) {
        sys_puts("  ", 0x07);
        sys_puts(applications[i], 0x0A);
        sys_putchar('\n');
    }
    return 0;
}

int main(int argc, char* argv[])
{
    char app_path[APP_PATH_MAX];
    char* app_argv[3];

    if (argc == 1)
        return list_applications();
    if (argc > 3) {
        sys_puts("Usage: open [app.APPL [file-or-directory]]\n", 0x0E);
        return 2;
    }
    if (!resolve_application(argv[1], app_path)) return 1;

    app_argv[0] = app_path;
    app_argv[1] = argc == 3 ? argv[2] : 0;
    app_argv[2] = 0;
    if (sys_exec(app_path, app_argv) < 0) {
        printf("open: cannot launch %s\n", app_path);
        return 1;
    }
    return 0;
}
