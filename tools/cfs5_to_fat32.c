// ============================================================
// CreateOS 0.0.1
// ============================================================

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "cfs5.h"

static FILE* image;
static cfs5_superblock_t superblock;
static char staging_root[PATH_MAX];

static int read_block(uint32_t block, void* buffer)
{
    if (block >= superblock.block_count || fseek(image, (long)block * CFS5_BLOCK_SIZE, SEEK_SET) != 0)
        return 0;
    return fread(buffer, CFS5_BLOCK_SIZE, 1, image) == 1;
}

static int read_inode(uint32_t number, cfs5_inode_t* inode)
{
    long offset;
    if (!inode || number >= superblock.inode_count)
        return 0;
    offset = (long)superblock.inode_table_start * CFS5_BLOCK_SIZE +
             (long)number * (long)sizeof(*inode);
    if (fseek(image, offset, SEEK_SET) != 0)
        return 0;
    return fread(inode, sizeof(*inode), 1, image) == 1;
}

static int valid_inode(const cfs5_inode_t* inode)
{
    if (!inode || inode->extent_count > CFS5_MAX_EXTENTS ||
        (inode->flags & CFS5_IF_COMPRESSED))
        return 0;
    for (uint32_t i = 0; i < inode->extent_count; i++) {
        uint64_t end = (uint64_t)inode->extents[i].start_block + inode->extents[i].block_count;
        if (!inode->extents[i].block_count || inode->extents[i].start_block < superblock.data_block_start ||
            end > superblock.block_count)
            return 0;
    }
    return 1;
}

static int safe_name(const char* name)
{
    return name && name[0] && strcmp(name, ".") != 0 && strcmp(name, "..") != 0 &&
           !strchr(name, '/') && !strchr(name, '\\');
}

static int make_path(char* out, size_t size, const char* parent, const char* name)
{
    int written = snprintf(out, size, "%s/%s", parent, name);
    return written > 0 && (size_t)written < size;
}

static int copy_file(const cfs5_inode_t* inode, const char* path)
{
    FILE* output;
    uint8_t block[CFS5_BLOCK_SIZE];
    uint32_t remaining = inode->size;
    output = fopen(path, "wb");
    if (!output) return 0;
    for (uint32_t e = 0; e < inode->extent_count && remaining; e++) {
        for (uint32_t b = 0; b < inode->extents[e].block_count && remaining; b++) {
            uint32_t amount = remaining > CFS5_BLOCK_SIZE ? CFS5_BLOCK_SIZE : remaining;
            if (!read_block(inode->extents[e].start_block + b, block) ||
                fwrite(block, 1, amount, output) != amount) {
                fclose(output);
                unlink(path);
                return 0;
            }
            remaining -= amount;
        }
    }
    if (fclose(output) != 0 || remaining != 0) {
        unlink(path);
        return 0;
    }
    chmod(path, inode->mode & 0777);
    return 1;
}

static int extract_directory(uint32_t inode_number, const char* path, unsigned depth)
{
    cfs5_inode_t inode;
    uint8_t block[CFS5_BLOCK_SIZE];
    if (depth > 64 || !read_inode(inode_number, &inode) || inode.type != CFS5_INODE_DIR ||
        !valid_inode(&inode)) return 0;
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return 0;
    for (uint32_t e = 0; e < inode.extent_count; e++) {
        for (uint32_t b = 0; b < inode.extents[e].block_count; b++) {
            if (!read_block(inode.extents[e].start_block + b, block)) return 0;
            cfs5_dirent_t* entries = (cfs5_dirent_t*)block;
            size_t count = CFS5_BLOCK_SIZE / sizeof(cfs5_dirent_t);
            for (size_t i = 0; i < count; i++) {
                char child[PATH_MAX];
                cfs5_inode_t child_inode;
                if (!entries[i].inode || !safe_name(entries[i].name)) continue;
                if (!read_inode(entries[i].inode, &child_inode) || !valid_inode(&child_inode) ||
                    !make_path(child, sizeof(child), path, entries[i].name)) return 0;
                if (child_inode.type == CFS5_INODE_DIR) {
                    if (!extract_directory(entries[i].inode, child, depth + 1)) return 0;
                } else if (child_inode.type == CFS5_INODE_FILE) {
                    if (!copy_file(&child_inode, child)) return 0;
                } else {
                    fprintf(stderr, "Unsupported CFS5 inode type for %s\n", child);
                    return 0;
                }
            }
        }
    }
    return 1;
}

static int run_mcopy(const char* destination)
{
    char command[PATH_MAX * 2 + 64];
    int written = snprintf(command, sizeof(command), "mcopy -s '%s'/* '%s'::/", staging_root, destination);
    if (written <= 0 || (size_t)written >= sizeof(command)) return 0;
    return system(command) == 0;
}

static void remove_tree(const char* path)
{
    char command[PATH_MAX + 32];
    int written = snprintf(command, sizeof(command), "rm -rf '%s'", path);
    if (written > 0 && (size_t)written < sizeof(command)) (void)system(command);
}

int main(int argc, char** argv)
{
    char template[] = "/tmp/createos-cfs5-XXXXXX";
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source.cfs5> <formatted-fat32.img>\n", argv[0]);
        return 2;
    }
    image = fopen(argv[1], "rb");
    if (!image || fread(&superblock, sizeof(superblock), 1, image) != 1 ||
        superblock.magic != CFS5_MAGIC || superblock.version != CFS5_VERSION ||
        superblock.block_size != CFS5_BLOCK_SIZE || superblock.root_inode >= superblock.inode_count) {
        fprintf(stderr, "Invalid CFS5 image: %s\n", argv[1]);
        if (image) fclose(image);
        return 1;
    }
    if (!mkdtemp(template)) {
        fclose(image);
        return 1;
    }
    strncpy(staging_root, template, sizeof(staging_root) - 1);
    staging_root[sizeof(staging_root) - 1] = 0;
    if (!extract_directory(superblock.root_inode, staging_root, 0) || !run_mcopy(argv[2])) {
        fprintf(stderr, "CFS5 to FAT32 conversion failed\n");
        remove_tree(staging_root);
        fclose(image);
        return 1;
    }
    remove_tree(staging_root);
    fclose(image);
    return 0;
}
