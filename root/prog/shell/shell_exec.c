#include "user_syscall.h"
#include "shell_modules.h"
#include "unistd.h"

#define MAX_ARGS 32
#define MAX_PATH 256

static int parse_line(char* line, char* argv[], int max_args)
{
    int argc = 0;
    if (!line || !argv || max_args < 1) return 0;
    char* p = line;

    while (*p && argc < max_args - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
    }
    argv[argc] = NULL;
    return argc;
}

static int open_executable(const char* path)
{
    return sys_access(path, X_OK) == 0;
}

static int has_bin_suffix(const char* name)
{
    size_t length = strlen(name);
    return length >= 4 && strcmp(name + length - 4, ".bin") == 0;
}

static int has_appl_suffix(const char* name)
{
    size_t length = strlen(name);
    const char* suffix;
    if (length < 5) return 0;
    suffix = name + length - 5;
    return suffix[0] == '.' &&
           ((!strcmp(suffix + 1, "APPL")) ||
            (!strcmp(suffix + 1, "appl")));
}

static int find_executable(const char* cmd, char* fullpath, int maxlen)
{
    if (!cmd || !cmd[0] || !fullpath || maxlen < 2) return 0;
    int contains_slash = strchr(cmd, '/') != NULL;
    int has_suffix = has_bin_suffix(cmd);
    if (contains_slash) {
        int length = snprintf(fullpath, maxlen, "%s", cmd);
        if (length >= 0 && length < maxlen && open_executable(fullpath)) return 1;
        if (!has_suffix) {
            length = snprintf(fullpath, maxlen, "%s.bin", cmd);
            if (length >= 0 && length < maxlen && open_executable(fullpath)) return 1;
        }
        return 0;
    }

    const char* paths[] = {"/bin", "/usr/bin", NULL};
    for (int pi = 0; paths[pi]; pi++) {
        int length = snprintf(fullpath, maxlen, "%s/%s%s", paths[pi], cmd,
                              has_suffix ? "" : ".bin");
        if (length >= 0 && length < maxlen && open_executable(fullpath)) return 1;
        if (!has_suffix) {
            length = snprintf(fullpath, maxlen, "%s/%s", paths[pi], cmd);
            if (length >= 0 && length < maxlen && open_executable(fullpath)) return 1;
        }
    }
    return 0;
}

int shell_execute_line(char* line)
{
    char* argv[MAX_ARGS];
    int argc = parse_line(line, argv, MAX_ARGS);
    if (argc < 1 || !argv[0] || !argv[0][0]) return -1;

    if (has_appl_suffix(argv[0])) {
        printf("%s: use 'open %s' to launch applications\n",
               argv[0], argv[0]);
        return -1;
    }

    char fullpath[MAX_PATH];
    if (!find_executable(argv[0], fullpath, MAX_PATH)) {
        printf("%s: command not found\n", argv[0]);
        return -1;
    }

    int result = sys_exec(fullpath, argv);
    printf("%s: exec failed (status %d)\n", argv[0], result);
    return -1;
}
