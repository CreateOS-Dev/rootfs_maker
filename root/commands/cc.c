#include "user_syscall.h"
#include <string.h>

#define CC_COMPILER_PATH "/app/system/bin/tcc.APPL"
#define CC_MAX_ARGUMENTS 31

int main(int argc, char* argv[])
{
    char* app_argv[CC_MAX_ARGUMENTS + 2];
    int count = 0;

    if (argc < 2) {
        printf("cc: no input files\n");
        printf("usage: cc file.c [-o output]\n");
        return 1;
    }

    app_argv[count++] = CC_COMPILER_PATH;
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-std=", 5) || !strcmp(argv[i], "-pipe"))
            continue;
        if (count >= CC_MAX_ARGUMENTS + 1) {
            printf("cc: too many arguments\n");
            return 2;
        }
        app_argv[count++] = argv[i];
    }
    app_argv[count] = 0;

    int status = sys_exec(CC_COMPILER_PATH, app_argv);
    printf("cc: could not start compiler (status %d)\n", status);
    return 1;
}