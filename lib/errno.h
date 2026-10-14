#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno;

#define EIO       5
#define EBADF     9
#define ECHILD    10
#define EAGAIN    11
#define ENOMEM    12
#define EACCES    13
#define EEXIST    17
#define EFAULT    14
#define ENOENT    2
#define ENOTDIR   20
#define EISDIR    21
#define EINVAL    22
#define EMFILE    24
#define ENOSPC    28
#define ESPIPE    29
#define ENOSYS    38

#endif
