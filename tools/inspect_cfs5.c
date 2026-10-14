#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cfs5.h"

static int read_block(FILE* image, const cfs5_superblock_t* super,
                      uint32_t block, void* buffer)
{
    if (block >= super->block_count) return 0;
    if (fseek(image, (long)block * CFS5_BLOCK_SIZE, SEEK_SET) != 0) return 0;
    return fread(buffer, CFS5_BLOCK_SIZE, 1, image) == 1;
}

static int read_inode(FILE* image, const cfs5_superblock_t* super,
                      uint32_t number, cfs5_inode_t* inode)
{
    uint32_t per_block = CFS5_BLOCK_SIZE / sizeof(cfs5_inode_t);
    uint32_t block = super->inode_table_start + number / per_block;
    uint32_t index = number % per_block;
    uint8_t buffer[CFS5_BLOCK_SIZE];
    if (number >= super->inode_count || !read_block(image, super, block, buffer))
        return 0;
    memcpy(inode, buffer + index * sizeof(*inode), sizeof(*inode));
    return 1;
}

static int find_child(FILE* image, const cfs5_superblock_t* super,
                      const cfs5_inode_t* directory, const char* wanted,
                      uint32_t* result)
{
    uint8_t buffer[CFS5_BLOCK_SIZE];
    if (directory->type != CFS5_INODE_DIR ||
        directory->extent_count > CFS5_MAX_EXTENTS) return 0;
    for (uint32_t e = 0; e < directory->extent_count; e++) {
        const cfs5_extent_t* extent = &directory->extents[e];
        if (!extent->block_count || extent->start_block >= super->block_count ||
            extent->block_count > super->block_count - extent->start_block)
            return 0;
        for (uint32_t b = 0; b < extent->block_count; b++) {
            if (!read_block(image, super, extent->start_block + b, buffer)) return 0;
            const cfs5_dirent_t* entries = (const cfs5_dirent_t*)buffer;
            uint32_t count = CFS5_BLOCK_SIZE / sizeof(cfs5_dirent_t);
            for (uint32_t i = 0; i < count; i++) {
                if (!entries[i].inode) continue;
                char name[CFS5_MAX_NAME + 1];
                memcpy(name, entries[i].name, CFS5_MAX_NAME);
                name[CFS5_MAX_NAME] = 0;
                if (!strcmp(name, wanted)) {
                    *result = entries[i].inode;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int resolve_path(FILE* image, const cfs5_superblock_t* super,
                        const char* path, uint32_t* result)
{
    char copy[256];
    char* save = NULL;
    char* token;
    uint32_t current = super->root_inode;
    if (!path || path[0] != '/' || strlen(path) >= sizeof(copy)) return 0;
    strcpy(copy, path);
    token = strtok_r(copy, "/", &save);
    while (token) {
        cfs5_inode_t directory;
        if (!read_inode(image, super, current, &directory) ||
            !find_child(image, super, &directory, token, &current)) return 0;
        token = strtok_r(NULL, "/", &save);
    }
    *result = current;
    return 1;
}

int main(int argc, char** argv)
{
    FILE* image;
    cfs5_superblock_t super;
    cfs5_inode_t directory;
    uint32_t inode_number;
    uint8_t buffer[CFS5_BLOCK_SIZE];
    if (argc != 3) {
        fprintf(stderr, "Usage: %s image.cfs5 /directory\n", argv[0]);
        return 2;
    }
    image = fopen(argv[1], "rb");
    if (!image) { perror(argv[1]); return 1; }
    if (fread(&super, sizeof(super), 1, image) != 1 ||
        super.magic != CFS5_MAGIC || super.version != CFS5_VERSION) {
        fprintf(stderr, "Invalid CFS5 superblock\n");
        fclose(image);
        return 1;
    }
    if (!resolve_path(image, &super, argv[2], &inode_number) ||
        !read_inode(image, &super, inode_number, &directory) ||
        directory.type != CFS5_INODE_DIR ||
        directory.extent_count > CFS5_MAX_EXTENTS) {
        fprintf(stderr, "Cannot resolve directory %s\n", argv[2]);
        fclose(image);
        return 1;
    }
    printf("inode=%u size=%u extents=%u\n", inode_number, directory.size,
           directory.extent_count);
    for (uint32_t e = 0; e < directory.extent_count; e++) {
        const cfs5_extent_t* extent = &directory.extents[e];
        printf("extent[%u]=block:%u count:%u\n", e, extent->start_block,
               extent->block_count);
        if (!extent->block_count || extent->start_block >= super.block_count ||
            extent->block_count > super.block_count - extent->start_block) {
            fprintf(stderr, "Invalid extent bounds\n");
            fclose(image);
            return 1;
        }
        for (uint32_t b = 0; b < extent->block_count; b++) {
            if (!read_block(image, &super, extent->start_block + b, buffer)) {
                fclose(image);
                return 1;
            }
            const cfs5_dirent_t* entries = (const cfs5_dirent_t*)buffer;
            uint32_t count = CFS5_BLOCK_SIZE / sizeof(cfs5_dirent_t);
            for (uint32_t i = 0; i < count; i++) {
                if (!entries[i].inode) continue;
                char name[CFS5_MAX_NAME + 1];
                cfs5_inode_t child;
                memcpy(name, entries[i].name, CFS5_MAX_NAME);
                name[CFS5_MAX_NAME] = 0;
                if (!read_inode(image, &super, entries[i].inode, &child)) {
                    fclose(image);
                    return 1;
                }
                printf("%6u type=%u mode=%04o size=%u %s\n", entries[i].inode,
                       child.type, child.mode, child.size, name);
            }
        }
    }
    fclose(image);
    return 0;
}
