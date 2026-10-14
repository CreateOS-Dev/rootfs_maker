#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/types.h>

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8

struct dirent {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

#ifdef __cplusplus
extern "C" {
#endif

ssize_t getdents64(int fd, void* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
