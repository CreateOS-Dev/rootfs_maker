#include "user_syscall.h"
#include "shell_modules.h"

void shell_prompt(const char* tty_name, const char* cwd)
{
    user_t current;
    const char* name = "guest";
    if (sys_user_current(&current) > 0)
        name = current.name;
    sys_puts(name, 0x0A);
    sys_puts("@CreateOS(", 0x07);
    sys_puts(tty_name, 0x0B);
    sys_puts("):", 0x07);
    sys_puts(cwd, 0x09);
    sys_puts("$ ", 0x0F);
}
