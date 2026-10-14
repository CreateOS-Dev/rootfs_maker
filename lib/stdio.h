// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

int putchar(int c);
int puts(const char *s);
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list arg);
int vsprintf(char *str, const char *format, va_list arg);
int vsnprintf(char *str, size_t size, const char *format, va_list arg);
int remove(const char* path);
int rename(const char* old_path, const char* new_path);

#ifdef __cplusplus
}
#endif

#endif