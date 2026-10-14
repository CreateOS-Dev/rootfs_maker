#ifndef CREATEOS_TCC_STDIO_H
#define CREATEOS_TCC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct createos_file {
    int descriptor;
    int error;
    int eof;
    int ungot_character;
    int has_ungot_character;
} FILE;

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE* fopen(const char* path, const char* mode);
FILE* fdopen(int descriptor, const char* mode);
int fclose(FILE* stream);
int fflush(FILE* stream);
size_t fread(void* buffer, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream);
int fseek(FILE* stream, long offset, int origin);
long ftell(FILE* stream);
int fputc(int character, FILE* stream);
int fputs(const char* string, FILE* stream);
int printf(const char* format, ...);
int vprintf(const char* format, va_list arguments);
int fprintf(FILE* stream, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list arguments);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
int ungetc(int character, FILE* stream);
char* fgets(char* buffer, int size, FILE* stream);
int sscanf(const char* input, const char* format, ...);

#endif
