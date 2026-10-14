// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
int rand(void);
void srand(unsigned int seed);
int atoi(const char *str);
void abort(void);

#ifdef __cplusplus
}
#endif

#endif