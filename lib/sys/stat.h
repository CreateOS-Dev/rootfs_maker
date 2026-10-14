#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

#define S_IFMT  0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)

struct stat {
    uint32_t st_type;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_size;
};

#ifdef __cplusplus
extern "C" {
#endif

int stat(const char* path, struct stat* status);
int fstat(int fd, struct stat* status);
int mkdir(const char* path, mode_t mode);
int chmod(const char* path, mode_t mode);
mode_t umask(mode_t mask);

#ifdef __cplusplus
}
#endif

#endif
