#ifndef CREATEOS_TCC_STRING_H
#define CREATEOS_TCC_STRING_H

#include <stddef.h>

size_t strlen(const char* string);
char* strcpy(char* destination, const char* source);
char* strncpy(char* destination, const char* source, size_t count);
char* strcat(char* destination, const char* source);
int strcmp(const char* left, const char* right);
int strncmp(const char* left, const char* right, size_t count);
char* strchr(const char* string, int character);
char* strrchr(const char* string, int character);
char* strstr(const char* haystack, const char* needle);
char* strdup(const char* string);
void* memcpy(void* destination, const void* source, size_t count);
void* memmove(void* destination, const void* source, size_t count);
void* memset(void* destination, int value, size_t count);
int memcmp(const void* left, const void* right, size_t count);

#endif
