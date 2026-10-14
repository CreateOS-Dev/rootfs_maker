#include "user_syscall.h"
#include "dirent.h"
#include "errno.h"
#include "fcntl.h"
#include "stdio.h"
#include "sys/stat.h"
#include "sys/wait.h"
#include "unistd.h"

#include <stdarg.h>

int errno;

static int posix_result(int64_t result)
{
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return (int)result;
}

static int posix_status(int64_t result)
{
    return result < 0 ? posix_result(result) : 0;
}

int open(const char* path, int flags, ...)
{
    unsigned int mode = 0;
    int64_t result;

    if (flags & O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, unsigned int);
        va_end(arguments);
    }
    result = sys_open(path, flags, (int)mode);
    return posix_result(result);
}

ssize_t read(int fd, void* buffer, size_t count)
{
    int64_t result = sys_read(fd, buffer, count);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return (ssize_t)result;
}

ssize_t write(int fd, const void* buffer, size_t count)
{
    int64_t result = sys_write(fd, buffer, count);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return (ssize_t)result;
}

int close(int fd)
{
    return posix_result(sys_close(fd));
}

int access(const char* path, int mode)
{
    return posix_result(sys_access(path, mode));
}

int chdir(const char* path)
{
    return posix_status(sys_chdir(path));
}

int unlink(const char* path)
{
    return posix_status(sys_unlink(path));
}

pid_t fork(void)
{
    return (pid_t)posix_result(sys_fork());
}

pid_t wait(int* status)
{
    return (pid_t)posix_result(sys_wait(status));
}

int mkdir(const char* path, mode_t mode)
{
    return posix_status(sys_mkdir(path, (int)mode));
}

int chmod(const char* path, mode_t mode)
{
    return posix_status(sys_chmod(path, (int)mode));
}

int remove(const char* path)
{
    return posix_status(sys_unlink(path));
}

int rename(const char* old_path, const char* new_path)
{
    return posix_status(sys_rename(old_path, new_path));
}

mode_t umask(mode_t mask)
{
    return (mode_t)sys_umask(mask);
}

off_t lseek(int fd, off_t offset, int whence)
{
    int64_t result = sys_seek(fd, offset, whence);
    if (result < 0) {
        errno = (int)-result;
        return (off_t)-1;
    }
    return (off_t)result;
}

static void posix_stat_mode(struct stat* status)
{
    status->st_mode &= ~S_IFMT;
    status->st_mode |= status->st_type == FS_TYPE_DIR ? S_IFDIR : S_IFREG;
}

int stat(const char* path, struct stat* status)
{
    int result;
    if (!status) {
        errno = EFAULT;
        return -1;
    }
    result = sys_stat(path, (fs_stat_t*)status);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    posix_stat_mode(status);
    return 0;
}

int fstat(int fd, struct stat* status)
{
    int result;
    if (!status) {
        errno = EFAULT;
        return -1;
    }
    result = sys_fstat(fd, (fs_stat_t*)status);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    posix_stat_mode(status);
    return 0;
}

ssize_t getdents64(int fd, void* buffer, size_t size)
{
    int64_t result = sys_getdents64(fd, buffer, size);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return (ssize_t)result;
}

char* getcwd(char* buffer, size_t size)
{
    int result = sys_getcwd(buffer, size);
    if (result < 0) {
        errno = -result;
        return NULL;
    }
    return buffer;
}
