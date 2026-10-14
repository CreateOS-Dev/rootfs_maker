#include "user_syscall.h"

#define LUA_APPLICATION_PATH "/app/system/bin/lua.APPL"
#define LUA_MAX_ARGUMENTS 32

int main(int argc, char* argv[])
{
    char* app_argv[LUA_MAX_ARGUMENTS + 1];

    if (argc > LUA_MAX_ARGUMENTS) {
        sys_puts("lua: too many arguments\n", 0x0C);
        return 2;
    }

    app_argv[0] = LUA_APPLICATION_PATH;
    for (int i = 1; i < argc; i++)
        app_argv[i] = argv[i];
    app_argv[argc] = 0;

    int status = sys_exec(LUA_APPLICATION_PATH, app_argv);
    printf("lua: could not start interpreter (status %d)\n", status);
    return 1;
}
