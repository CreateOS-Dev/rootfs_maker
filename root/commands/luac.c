#include "user_syscall.h"

#define LUAC_APPLICATION_PATH "/app/system/bin/luac.APPL"
#define LUAC_MAX_ARGUMENTS 32

int main(int argc, char* argv[])
{
    char* app_argv[LUAC_MAX_ARGUMENTS + 1];

    if (argc > LUAC_MAX_ARGUMENTS) {
        sys_puts("luac: too many arguments\n", 0x0C);
        return 2;
    }

    app_argv[0] = LUAC_APPLICATION_PATH;
    for (int i = 1; i < argc; i++)
        app_argv[i] = argv[i];
    app_argv[argc] = 0;

    int status = sys_exec(LUAC_APPLICATION_PATH, app_argv);
    printf("luac: could not start compiler (status %d)\n", status);
    return 1;
}
