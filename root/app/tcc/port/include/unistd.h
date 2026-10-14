#ifndef CREATEOS_TCC_UNISTD_H
#define CREATEOS_TCC_UNISTD_H

#include <sys/types.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int close(int descriptor);
ssize_t read(int descriptor, void* buffer, size_t count);
ssize_t write(int descriptor, const void* buffer, size_t count);
off_t lseek(int descriptor, off_t offset, int origin);
int unlink(const char* path);
int access(const char* path, int mode);
int isatty(int descriptor);
int execvp(const char* file, char* const arguments[]);
void _exit(int status);

#endif
