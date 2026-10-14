// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * Command registration framework for CreateOS shell.
 * Each command implementation includes this header and registers itself via the REGISTER_CMD macro.
 * The macro places a cmd_entry_t in a custom linker section ".cmds".
 * At runtime, shell_init_commands() scans the section and builds a table of enabled commands.
 */

#ifndef CMD_REGISTRY_H
#define CMD_REGISTRY_H

#include <stddef.h>

typedef void (*cmd_handler_t)(char *args);

typedef struct {
    const char *name;          /* command name as typed */
    cmd_handler_t handler;     /* function to invoke */
    const char* (*help_func)(void); /* function to get help string */
} cmd_entry_t;

/*
 * REGISTER_CMD places a cmd_entry_t into the .cmds section.
 * The "used" attribute prevents the compiler from discarding it.
 * It also expects the command file to have a function named `cmd_<func>_help`
 * that returns the help string for that command.
 */
#define REGISTER_CMD(cmd_name, func, help_str)                     \
    static const cmd_entry_t __cmd_entry_##func               \
        __attribute__((section(".cmds"), used)) = {           \
            .name = cmd_name,                                 \
            .handler = func,                                  \
            .help_func = (const char* (*)(void))(&func##_help) \
        };

/* Extern symbols provided by the linker for the start/end of the .cmds section */
extern const cmd_entry_t __start_cmds[];
extern const cmd_entry_t __stop_cmds[];

#endif // CMD_REGISTRY_H