#include "user_syscall.h"

#define CAT_CHUNK_SIZE 512

static int print_file(const char* path)
{
    char buffer[CAT_CHUNK_SIZE];
    fs_stat_t status;
    uint32_t offset = 0;

    if (sys_stat(path, &status) < 0) {
        printf("cat: cannot access %s\n", path);
        return 1;
    }
    if (status.type != FS_TYPE_FILE) {
        printf("cat: %s is not a regular file\n", path);
        return 1;
    }

    while (offset < status.size) {
        size_t requested = status.size - offset;
        ssize_t count;
        if (requested > sizeof(buffer)) requested = sizeof(buffer);
        count = sys_read_at(path, buffer, requested, offset);
        if (count <= 0 || (size_t)count > requested) {
            printf("\ncat: read failed for %s at offset %u\n", path, offset);
            return 1;
        }
        for (ssize_t i = 0; i < count; i++) sys_putchar(buffer[i]);
        offset += (uint32_t)count;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    int failed = 0;
    if (argc < 2) {
        sys_puts("Usage: cat <file> [file ...]\n", 0x0E);
        return 2;
    }
    for (int i = 1; i < argc; i++)
        if (print_file(argv[i]) != 0) failed = 1;
    return failed;
}
