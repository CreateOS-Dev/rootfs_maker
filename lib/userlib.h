// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef _USERLIB_H
#define _USERLIB_H

#include <stddef.h>
#include <stdarg.h>

/* ============================================================
 * Required type definitions
 * ============================================================ */
typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

/* ============================================================
 * String functions (string.h)
 * ============================================================ */
size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
void* memchr(const void* s, int c, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);

/* ============================================================
 * Character type functions (ctype.h)
 * ============================================================ */
int isalnum(int c);
int isalpha(int c);
int isascii(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);

/* ============================================================
 * String functions (string.h) - extended
 * ============================================================ */
int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, size_t n);

/* ============================================================
 * Standard I/O functions (stdio.h)
 * ============================================================ */
int putchar(int c);
int puts(const char* s);
int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vprintf(const char* format, va_list arg);
int vsprintf(char* str, const char* format, va_list arg);
int vsnprintf(char* str, size_t size, const char* format, va_list arg);

/* ============================================================
 * Standard library functions (stdlib.h)
 * ============================================================ */
int atoi(const char* str);
long atol(const char* str);
long long atoll(const char* str);
double atof(const char* str);
int rand(void);
void srand(unsigned int seed);
void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
void exit(int status);
int atoi(const char* str);
long strtol(const char* str, char** endptr, int base);
unsigned long strtoul(const char* str, char** endptr, int base);
long long strtoll(const char* str, char** endptr, int base);
unsigned long long strtoull(const char* str, char** endptr, int base);
int rand(void);
void srand(unsigned int seed);
int abs(int j);
long labs(long j);
long long llabs(long long j);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

#endif // _USERLIB_H