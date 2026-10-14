#ifndef CREATEOS_TCC_STDLIB_H
#define CREATEOS_TCC_STDLIB_H

#include <stddef.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void* realloc(void* pointer, size_t size);
void free(void* pointer);
void abort(void);
void exit(int status);
char* getenv(const char* name);
long strtol(const char* text, char** end, int base);
unsigned long strtoul(const char* text, char** end, int base);
long long strtoll(const char* text, char** end, int base);
unsigned long long strtoull(const char* text, char** end, int base);
double strtod(const char* text, char** end);
float strtof(const char* text, char** end);
long double strtold(const char* text, char** end);
int atoi(const char* text);
int abs(int value);
void qsort(void* base, size_t count, size_t width,
           int (*compare)(const void*, const void*));

#endif
