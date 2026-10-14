#ifndef CREATEOS_SHELL_MODULES_H
#define CREATEOS_SHELL_MODULES_H

void shell_prompt(const char* tty_name, const char* cwd);
int shell_execute_line(char* line);
void shell_history_load(void);
void shell_history_add(const char* line);
void shell_history_reset(void);
int shell_history_previous(char* buffer, size_t size);
int shell_history_next(char* buffer, size_t size);

#endif
