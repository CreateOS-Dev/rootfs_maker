#ifndef CREATEOS_LUA_COMPAT_H
#define CREATEOS_LUA_COMPAT_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define L_tmpnam 128
#define LUA_PATH_DEFAULT \
    "./?.lua;./?/init.lua;/usr/share/lua/5.4/?.lua;" \
    "/usr/share/lua/5.4/?/init.lua"
#define LUA_CPATH_DEFAULT ""

void* realloc(void* pointer, size_t size);
void exit(int status);
char* getenv(const char* name);
int system(const char* command);
int remove(const char* path);
int rename(const char* old_path, const char* new_path);
char* tmpnam(char* buffer);
void* memchr(const void* memory, int value, size_t length);
int iscntrl(int character);
int isgraph(int character);
int ispunct(int character);
int isxdigit(int character);
void createos_lua_write(const char* bytes, size_t length);
void createos_lua_write_error(const char* format, const char* message);
char* createos_lua_strpbrk(const char* string, const char* accept);
size_t createos_lua_strspn(const char* string, const char* accept);
double createos_lua_strtod(const char* string, char** end);
int createos_lua_strcoll(const char* left, const char* right);
int createos_lua_snprintf(char* output, size_t capacity,
                          const char* format, ...);
int createos_lua_integer_to_string(char* output, size_t capacity,
                                   long long value);
int createos_lua_number_to_string(char* output, size_t capacity,
                                  double value);
int createos_lua_abs(int value);
unsigned int createos_lua_randomize_pivot(void);
struct lua_State;
int luaL_loadfilex(struct lua_State* state, const char* path,
                   const char* mode);

#define abs createos_lua_abs
#define strpbrk createos_lua_strpbrk
#define strspn createos_lua_strspn
#define strtod createos_lua_strtod
#define strcoll createos_lua_strcoll
#define snprintf createos_lua_snprintf
#define lua_getlocaledecpoint() '.'
#define lua_writestring(string, length) createos_lua_write((string), (length))
#define lua_writeline() createos_lua_write("\n", 1)
#define lua_writestringerror(format, message) \
    createos_lua_write_error((format), (message))

#endif
