#include "user_syscall.h"

#define TCC_APPLICATION_PATH "/app/system/bin/tcc.APPL"
#define TCC_MAX_ARGUMENTS 32

int main(int argc, char* argv[])
{
    char* app_argv[TCC_MAX_ARGUMENTS + 1];

    if (argc > TCC_MAX_ARGUMENTS) {
        sys_puts("tcc: too many arguments\n", 0x0C);
        return 2;
    }

    app_argv[0] = TCC_APPLICATION_PATH;
    for (int i = 1; i < argc; i++)
        app_argv[i] = argv[i];
    app_argv[argc] = 0;

    int status = sys_exec(TCC_APPLICATION_PATH, app_argv);
    printf("tcc: could not start compiler (status %d)\n", status);
    return 1;
}
