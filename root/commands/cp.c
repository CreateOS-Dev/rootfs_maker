#include "user_syscall.h"

static int copy_file(const char* source, const char* destination)
{
    fs_stat_t source_stat;
    char buffer[4096];
    uint64_t copied = 0;
    int source_fd;
    int destination_fd;
    int failed = 0;

    if (strcmp(source, destination) == 0) {
        printf("cp: source and destination are the same path\n");
        return 0;
    }
    if (sys_stat(source, &source_stat) != 0 ||
        source_stat.type != FS_TYPE_FILE) {
        printf("cp: source is not a readable regular file: %s\n", source);
        return 0;
    }
    source_fd = sys_open(source, O_RDONLY, 0);
    if (source_fd < 0) {
        printf("cp: cannot open source: %s\n", source);
        return 0;
    }
    destination_fd = sys_open(destination,
                              O_WRONLY | O_CREAT | O_TRUNC,
                              source_stat.mode & 0777);
    if (destination_fd < 0) {
        sys_close(source_fd);
        printf("cp: cannot create destination: %s\n", destination);
        return 0;
    }

    for (;;) {
        ssize_t count = sys_read(source_fd, buffer, sizeof(buffer));
        if (count < 0 || count > (ssize_t)sizeof(buffer)) {
            printf("cp: read failed for %s\n", source);
            failed = 1;
            break;
        }
        if (count == 0) break;

        size_t written = 0;
        while (written < (size_t)count) {
            ssize_t amount = sys_write(destination_fd, buffer + written,
                                       (size_t)count - written);
            if (amount <= 0 || amount > count - (ssize_t)written) {
                printf("cp: write failed for %s\n", destination);
                failed = 1;
                break;
            }
            written += (size_t)amount;
            copied += (uint64_t)amount;
        }
        if (failed) break;
    }

    if (!failed && copied != source_stat.size) {
        printf("cp: source size changed during copy (%u bytes expected, "
               "%u bytes copied)\n",
               source_stat.size, (unsigned int)copied);
        failed = 1;
    }
    if (sys_close(source_fd) < 0) {
        printf("cp: failed to close source %s\n", source);
        failed = 1;
    }
    if (sys_close(destination_fd) < 0) {
        printf("cp: failed to close destination %s\n", destination);
        failed = 1;
    }
    if (!failed && sys_sync() < 0) {
        printf("cp: storage sync failed; destination may not be durable\n");
        failed = 1;
    }
    if (failed)
        sys_puts("cp: destination may contain incomplete data\n", 0x0C);
    return !failed;
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        sys_puts("Usage: cp <source-file> <destination-file>\n", 0x0E);
        sys_puts("Mounted FAT32 destinations accept 8.3 filenames only.\n",
                 0x0E);
        return 2;
    }
    return copy_file(argv[1], argv[2]) ? 0 : 1;
}
