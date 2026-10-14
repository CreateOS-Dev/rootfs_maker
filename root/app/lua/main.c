#include "user_syscall.h"
#include "createos/uikit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define LUA_APP_MAX_FILE_SIZE (1024u * 1024u)
#define LUA_REPL_BUFFER_SIZE (8u * 1024u)

static createos_ui_t lua_ui;

static void* lua_allocator(void* context, void* pointer, size_t old_size,
                           size_t new_size)
{
    void* replacement;
    (void)context;
    if (!new_size) {
        free(pointer);
        return 0;
    }
    replacement = malloc(new_size);
    if (!replacement) return 0;
    if (pointer) {
        size_t copy_size = old_size < new_size ? old_size : new_size;
        memcpy(replacement, pointer, copy_size);
        free(pointer);
    }
    return replacement;
}

static int load_script(lua_State* state, const char* path)
{
    fs_stat_t status;
    char* source;
    ssize_t count;
    int result;

    if (sys_stat(path, &status) < 0 || status.type != FS_TYPE_FILE ||
        status.size > LUA_APP_MAX_FILE_SIZE) {
        printf("lua: cannot read script or file is too large: %s\n", path);
        return LUA_ERRFILE;
    }
    source = malloc((size_t)status.size + 1);
    if (!source) {
        sys_puts("lua: not enough memory to load script\n", 0x0C);
        return LUA_ERRMEM;
    }
    count = sys_read_at(path, source, status.size, 0);
    if (count < 0 || (uint32_t)count != status.size) {
        free(source);
        printf("lua: failed to read script: %s\n", path);
        return LUA_ERRFILE;
    }
    source[status.size] = 0;
    result = luaL_loadbufferx(state, source, status.size, path, 0);
    free(source);
    return result;
}

static int report_lua_error(lua_State* state, int status)
{
    const char* message = lua_tostring(state, -1);
    printf("lua: %s\n", message ? message : "unknown interpreter error");
    lua_pop(state, 1);
    return status ? status : 1;
}

static int run_loaded_chunk(lua_State* state, int status)
{
    if (status != LUA_OK) return report_lua_error(state, status);
    status = lua_pcall(state, 0, LUA_MULTRET, 0);
    return status == LUA_OK ? 0 : report_lua_error(state, status);
}

static void open_supported_libraries(lua_State* state)
{
    luaL_openlibs(state);
    lua_getglobal(state, "package");
    if (lua_istable(state, -1)) {
        user_t user;
        char module_path[256];
        if (sys_user_current(&user) == 0 &&
            snprintf(module_path, sizeof(module_path),
                     "%s/?.lua;%s/?/init.lua;./?.lua;./?/init.lua;"
                     "/usr/share/lua/5.4/?.lua;"
                     "/usr/share/lua/5.4/?/init.lua",
                     user.home, user.home) > 0) {
            lua_pushstring(state, module_path);
            lua_setfield(state, -2, "path");
        }
        lua_pushliteral(state, "");
        lua_setfield(state, -2, "cpath");
    }
    lua_pop(state, 1);
}

static int initialize_libraries(lua_State* state)
{
    open_supported_libraries(state);
    return 0;
}

static int read_repl_line(char* buffer, size_t capacity)
{
    size_t length = 0;
    int overflow = 0;
    for (;;) {
        int key = createos_ui_read_key();
        if (key == '\r' || key == '\n') {
            if (overflow) {
                buffer[0] = 0;
                sys_puts("\nlua: input line is too long\n", 0x0C);
                return -2;
            }
            buffer[length] = 0;
            sys_puts("\n", 0x07);
            return (int)length;
        }
        if (key == 4 || key == KEY_INTERRUPT || key == KEY_CTRL_C ||
            key == 27) {
            buffer[0] = 0;
            sys_puts("\n", 0x07);
            return -1;
        }
        if (key == 8 || key == 127) {
            if (length) {
                length--;
                sys_puts("\b \b", 0x07);
            }
            continue;
        }
        if (key < 32 || key > 126)
            continue;
        if (length + 1 >= capacity) {
            overflow = 1;
            continue;
        }
        buffer[length++] = (char)key;
        {
            char echo[2] = {(char)key, 0};
            sys_puts(echo, 0x07);
        }
    }
}

static int run_repl(lua_State* state)
{
    char line[LUA_REPL_BUFFER_SIZE];
    for (;;) {
        int length;
        sys_puts("> ", 0x0B);
        length = read_repl_line(line, sizeof(line));
        if (length == -1) return 0;
        if (length == -2) continue;
        if (!length) continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
            return 0;
        if (line[0] == '=') {
            if ((size_t)length > sizeof(line) - 7) {
                sys_puts("lua: expression exceeds the REPL buffer\n", 0x0C);
                continue;
            }
            memmove(line + 7, line + 1, strlen(line + 1) + 1);
            memcpy(line, "return ", 7);
        }
        for (;;) {
            int result = luaL_loadbufferx(state, line, (size_t)strlen(line),
                                          "=stdin", 0);
            if (result != LUA_OK) {
                const char* message = lua_tostring(state, -1);
                if (result != LUA_ERRSYNTAX || !message ||
                    !strstr(message, "<eof>")) {
                    report_lua_error(state, result);
                    break;
                }
                lua_pop(state, 1);
                if ((size_t)length + 1 >= sizeof(line)) {
                    sys_puts("lua: incomplete input exceeds the REPL buffer\n",
                             0x0C);
                    break;
                }
                line[length++] = '\n';
                line[length] = 0;
                sys_puts(">> ", 0x0B);
                int added = read_repl_line(line + length,
                                           sizeof(line) - (size_t)length);
                if (added == -1) return 0;
                if (added == -2) break;
                length += added;
                continue;
            }
            result = lua_pcall(state, 0, LUA_MULTRET, 0);
            if (result != LUA_OK) {
                report_lua_error(state, result);
                break;
            }
            int results = lua_gettop(state);
            if (results) {
                for (int i = 1; i <= results; i++) {
                    const char* output = luaL_tolstring(state, i, 0);
                    if (i > 1) sys_puts("\t", 0x07);
                    sys_puts(output ? output : "nil", 0x07);
                    lua_pop(state, 1);
                }
                sys_puts("\n", 0x07);
                lua_settop(state, 0);
            }
            break;
        }
    }
}

int main(int argc, char* argv[])
{
    lua_State* state;
    int result = 0;
    if (createos_ui_init(&lua_ui) < 0) {
        sys_puts("lua: could not initialize console\n", 0x0C);
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "-v") == 0) {
        printf("%s on CreateOS\n", LUA_RELEASE);
        createos_ui_shutdown();
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "-e") != 0) {
        sys_puts("Usage: lua.APPL [-v] | [-e code] | [script.lua]\n", 0x0E);
        createos_ui_shutdown();
        return 2;
    }
    state = lua_newstate(lua_allocator, 0);
    if (!state) {
        sys_puts("lua: interpreter state allocation failed\n", 0x0C);
        createos_ui_shutdown();
        return 1;
    }
    lua_pushcfunction(state, initialize_libraries);
    result = lua_pcall(state, 0, 0, 0);
    if (result != LUA_OK) {
        result = report_lua_error(state, result);
        lua_close(state);
        (void)createos_ui_shutdown();
        return result;
    }
    if (argc == 1) {
        sys_puts(LUA_RELEASE " for CreateOS; type exit to quit.\n", 0x0B);
        result = run_repl(state);
    } else if (argc == 3 && strcmp(argv[1], "-e") == 0) {
        result = run_loaded_chunk(state,
            luaL_loadstring(state, argv[2]));
    } else if (argc == 2) {
        result = run_loaded_chunk(state, load_script(state, argv[1]));
    } else {
        sys_puts("Usage: lua.APPL [-v] | [-e code] | [script.lua]\n", 0x0E);
        result = 2;
    }
    lua_close(state);
    (void)createos_ui_shutdown();
    return result;
}
