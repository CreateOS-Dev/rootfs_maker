// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#ifdef __cplusplus
extern "C" {
#endif

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
char* getcwd(char *buf, size_t size);
int access(const char* path, int mode);
int chdir(const char* path);
int unlink(const char* path);
pid_t fork(void);

#ifdef __cplusplus
}
#endif

#endif