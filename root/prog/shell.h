// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef SHELL_H
#define SHELL_H

#define MAX_CMDS 128
#define MAX_EXPORTS 32

typedef struct {
    const char* name;
    void (*handler)(char *args);
    const char* (*help_func)(void); /* function to get help string */
    int enabled;
} cmd_state_t;

typedef struct {
    char name[32];
    char value[96];
} export_t;

extern int g_cmd_count;
extern cmd_state_t g_cmd_states[MAX_CMDS];

extern int g_export_count;
extern export_t g_exports[MAX_EXPORTS];

void shell_run(void);
void shell_reset_state(void);
void build_path(const char* arg, char* out, int max);
void normalize_path_shell(char* path);
int path_allowed(const char* path);

#endif // SHELL_H