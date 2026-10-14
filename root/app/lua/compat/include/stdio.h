#ifndef CREATEOS_LUA_STDIO_H
#define CREATEOS_LUA_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define BUFSIZ 1024
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

typedef struct createos_lua_FILE FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);
size_t fread(void* buffer, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream);
int ferror(FILE* stream);
int feof(FILE* stream);
int fflush(FILE* stream);
int fgetc(FILE* stream);
int getc(FILE* stream);
int ungetc(int character, FILE* stream);
void clearerr(FILE* stream);
int fseek(FILE* stream, long offset, int origin);
long ftell(FILE* stream);
int setvbuf(FILE* stream, char* buffer, int mode, size_t size);
FILE* tmpfile(void);
int fputc(int character, FILE* stream);
int fputs(const char* string, FILE* stream);
char* fgets(char* string, int size, FILE* stream);
int vfprintf(FILE* stream, const char* format, va_list arguments);
int fprintf(FILE* stream, const char* format, ...);
FILE* freopen(const char* path, const char* mode, FILE* stream);
char* strerror(int error);

#endif
