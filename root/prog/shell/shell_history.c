#include "user_syscall.h"
#include "shell_modules.h"

#define MAX_INPUT 256
#define MAX_HISTORY 64

static char history[MAX_HISTORY][MAX_INPUT];
static int history_count;
static int history_position = -1;

void shell_history_load(void)
{
    int count = sys_history_count();
    if (count < 0) {
        printf("shell: unable to load command history\n");
        return;
    }
    if (count > MAX_HISTORY) count = MAX_HISTORY;
    for (int i = 0; i < count; i++) {
        if (sys_history_get((unsigned)i, history[i], sizeof(history[i])) < 0) {
            printf("shell: unable to load command history entry\n");
            history_count = 0;
            return;
        }
    }
    history_count = count;
}

void shell_history_add(const char* line)
{
    if (!line[0]) return;
    if (history_count > 0 &&
        strcmp(history[history_count - 1], line) == 0)
        return;

    if (sys_history_add(line) < 0)
        printf("shell: unable to save command history\n");

    if (history_count < MAX_HISTORY) {
        strcpy(history[history_count++], line);
    } else {
        for (int i = 1; i < MAX_HISTORY; i++)
            strcpy(history[i - 1], history[i]);
        strcpy(history[MAX_HISTORY - 1], line);
    }
    history_position = -1;
}

void shell_history_reset(void)
{
    history_position = -1;
}

static void history_copy(char* buffer, size_t size, const char* entry)
{
    size_t length = strlen(entry);
    if (length >= size) length = size - 1;
    memcpy(buffer, entry, length);
    buffer[length] = 0;
}

int shell_history_previous(char* buffer, size_t size)
{
    if (!buffer || size == 0 || history_position >= history_count - 1)
        return 0;
    history_position++;
    history_copy(buffer, size,
                 history[history_count - 1 - history_position]);
    return 1;
}

int shell_history_next(char* buffer, size_t size)
{
    if (!buffer || size == 0 || history_position < 0)
        return 0;
    if (history_position > 0) {
        history_position--;
        history_copy(buffer, size,
                     history[history_count - 1 - history_position]);
    } else {
        history_position = -1;
        buffer[0] = 0;
    }
    return 1;
}
