// ============================================================
// CreateOS 0.0.1
// ============================================================

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include "cfs5.h"

#define CFS5_MAGIC          0x43534635  // "CSF5"
#define CFS5_VERSION        1
#define CFS5_BLOCK_SIZE     4096
#define CFS5_MAX_NAME       64

/* Read the superblock from the image */
static bool read_superblock(FILE* img, cfs5_superblock_t* sb) {
    fseek(img, 0, SEEK_SET);
    size_t ret = fread(sb, sizeof(cfs5_superblock_t), 1, img);
    return ret == 1 && sb->magic == CFS5_MAGIC && sb->version == CFS5_VERSION;
}

/* Read a block from the image */
static bool read_block(FILE* img, uint32_t block, void* buf) {
    fseek(img, block * CFS5_BLOCK_SIZE, SEEK_SET);
    size_t ret = fread(buf, CFS5_BLOCK_SIZE, 1, img);
    return ret == 1;
}

/* Write a block to the image */
static bool write_block(FILE* img, uint32_t block, const void* buf) {
    fseek(img, block * CFS5_BLOCK_SIZE, SEEK_SET);
    size_t ret = fwrite(buf, CFS5_BLOCK_SIZE, 1, img);
    fflush(img);
    return ret == 1;
}

/* Allocate a free block from the bitmap */
static uint32_t allocate_block(const cfs5_superblock_t *sb, FILE* img) {
    uint32_t bitmap_block = sb->free_block_bitmap;
    uint8_t bitmap[CFS5_BLOCK_SIZE];

    fprintf(stderr, "DEBUG: allocate_block: bitmap_block=%u, data_block_start=%u\n", bitmap_block, sb->data_block_start);

    if (!read_block(img, bitmap_block, bitmap)) {
        fprintf(stderr, "DEBUG: allocate_block: failed to read bitmap block %u\n", bitmap_block);
        return 0;
    }

    // Let's check the first few bytes of the bitmap to see if it's really all zeros
    for (int i = 0; i < 8 && i < CFS5_BLOCK_SIZE; i++) {
        fprintf(stderr, "DEBUG: allocate_block: bitmap[%d] = 0x%02x\n", i, bitmap[i]);
    }

    for (int i = 0; i < CFS5_BLOCK_SIZE * 8; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            bitmap[byte_idx] |= (1 << bit_idx);
            if (!write_block(img, bitmap_block, bitmap)) {
                fprintf(stderr, "DEBUG: allocate_block: failed to write bitmap block %u\n", bitmap_block);
                return 0;
            }
            uint32_t block_no = sb->data_block_start + i;
            fprintf(stderr, "DEBUG: allocate_block: allocating block %u (bit %d)\n", block_no, i);
            return block_no;
        }
    }
    return 0; // No free blocks
}

static uint32_t allocate_block_run(const cfs5_superblock_t* sb, FILE* img,
                                   uint32_t max_count, uint32_t* out_start)
{
    const uint32_t bits_per_bitmap = CFS5_BLOCK_SIZE * 8U;
    uint8_t bitmap[CFS5_BLOCK_SIZE];
    uint32_t best_bitmap = 0;
    uint32_t best_bit = 0;
    uint32_t best_count = 0;

    if (!sb || !out_start || !max_count ||
        sb->data_block_start >= sb->block_count)
        return 0;

    for (uint32_t map = 0; map < sb->free_block_bitmap_blocks; map++) {
        uint32_t run_start = 0;
        uint32_t run_count = 0;
        if (!read_block(img, sb->free_block_bitmap + map, bitmap))
            return 0;

        for (uint32_t bit = 0; bit < bits_per_bitmap; bit++) {
            uint64_t relative = (uint64_t)map * bits_per_bitmap + bit;
            int available = relative < sb->block_count - sb->data_block_start &&
                !(bitmap[bit / 8] & (1U << (bit % 8)));
            if (!available) {
                if (run_count > best_count) {
                    best_bitmap = map;
                    best_bit = run_start;
                    best_count = run_count;
                }
                run_count = 0;
                continue;
            }
            if (!run_count)
                run_start = bit;
            if (++run_count == max_count) {
                best_bitmap = map;
                best_bit = run_start;
                best_count = run_count;
                goto found;
            }
        }
        if (run_count > best_count) {
            best_bitmap = map;
            best_bit = run_start;
            best_count = run_count;
        }
    }

found:
    if (!best_count)
        return 0;
    if (!read_block(img, sb->free_block_bitmap + best_bitmap, bitmap))
        return 0;
    uint64_t first_relative = (uint64_t)best_bitmap * bits_per_bitmap +
                              best_bit;
    uint64_t remaining_blocks =
        (uint64_t)sb->block_count - sb->data_block_start - first_relative;
    if (best_count > remaining_blocks)
        best_count = (uint32_t)remaining_blocks;
    for (uint32_t i = 0; i < best_count; i++) {
        uint32_t bit = best_bit + i;
        bitmap[bit / 8] |= (uint8_t)(1U << (bit % 8));
    }
    if (!write_block(img, sb->free_block_bitmap + best_bitmap, bitmap))
        return 0;
    *out_start = sb->data_block_start + (uint32_t)first_relative;
    return best_count;
}

static bool release_block(const cfs5_superblock_t* sb, FILE* img, uint32_t block)
{
    uint8_t bitmap[CFS5_BLOCK_SIZE];
    uint32_t relative;
    uint32_t bitmap_block_offset;
    uint32_t bit_offset;
    if (block < sb->data_block_start) return false;
    relative = block - sb->data_block_start;
    bitmap_block_offset = relative / (CFS5_BLOCK_SIZE * 8U);
    bit_offset = relative % (CFS5_BLOCK_SIZE * 8U);
    if (bitmap_block_offset >= sb->free_block_bitmap_blocks ||
        !read_block(img, sb->free_block_bitmap + bitmap_block_offset, bitmap))
        return false;
    bitmap[bit_offset / 8] &= (uint8_t)~(1U << (bit_offset % 8));
    return write_block(img, sb->free_block_bitmap + bitmap_block_offset, bitmap);
}

static bool read_inode(FILE* img, const cfs5_superblock_t* sb, uint32_t ino,
                       cfs5_inode_t* in);
static bool write_inode(FILE* img, const cfs5_superblock_t* sb, uint32_t ino,
                        const cfs5_inode_t* in);

static bool file_contents_match(FILE* img, const cfs5_superblock_t* sb,
                                const cfs5_inode_t* inode,
                                const uint8_t* data, uint32_t size,
                                bool* matches)
{
    uint8_t block_buf[CFS5_BLOCK_SIZE];
    uint32_t remaining = size;
    uint32_t data_offset = 0;

    *matches = false;
    if (inode->type != CFS5_INODE_FILE ||
        inode->extent_count > CFS5_MAX_EXTENTS)
        return false;
    if (inode->size != size) return true;
    if (!size) {
        *matches = inode->extent_count == 0;
        return true;
    }

    for (uint32_t i = 0; i < inode->extent_count && remaining; i++) {
        const cfs5_extent_t* extent = &inode->extents[i];
        if (!extent->block_count || extent->start_block < sb->data_block_start ||
            extent->start_block >= sb->block_count ||
            extent->block_count > sb->block_count - extent->start_block)
            return false;
        for (uint32_t b = 0; b < extent->block_count && remaining; b++) {
            uint32_t chunk = remaining < CFS5_BLOCK_SIZE ?
                             remaining : CFS5_BLOCK_SIZE;
            if (!read_block(img, extent->start_block + b, block_buf))
                return false;
            if (memcmp(block_buf, data + data_offset, chunk) != 0) return true;
            remaining -= chunk;
            data_offset += chunk;
        }
    }
    if (remaining) return false;
    *matches = true;
    return true;
}

static bool replace_file_contents(FILE* img, const cfs5_superblock_t* sb,
                                  uint32_t file_ino, const uint8_t* data,
                                  size_t size, const char* path)
{
    cfs5_inode_t old_inode;
    cfs5_inode_t new_inode;
    uint32_t* blocks = NULL;
    uint32_t blocks_needed;
    uint32_t allocated = 0;
    bool matches;

    if (size > UINT32_MAX ||
        !read_inode(img, sb, file_ino, &old_inode) ||
        old_inode.type != CFS5_INODE_FILE ||
        old_inode.extent_count > CFS5_MAX_EXTENTS) {
        fprintf(stderr, "Invalid destination file inode for %s\n", path);
        return false;
    }
    uint64_t old_block_total = 0;
    for (uint32_t i = 0; i < old_inode.extent_count; i++) {
        const cfs5_extent_t* extent = &old_inode.extents[i];
        if (!extent->block_count || extent->start_block < sb->data_block_start ||
            extent->start_block >= sb->block_count ||
            extent->block_count > sb->block_count - extent->start_block) {
            fprintf(stderr, "Invalid data extent in destination file %s\n", path);
            return false;
        }
        old_block_total += extent->block_count;
    }
    if (old_block_total !=
        ((uint64_t)old_inode.size + CFS5_BLOCK_SIZE - 1) / CFS5_BLOCK_SIZE) {
        fprintf(stderr, "Inconsistent allocated size for destination file %s\n",
                path);
        return false;
    }
    if (!file_contents_match(img, sb, &old_inode, data, (uint32_t)size,
                             &matches)) {
        fprintf(stderr, "Cannot safely inspect existing file %s\n", path);
        return false;
    }
    if (matches) {
        printf("File %s is already up to date\n", path);
        return true;
    }

    blocks_needed = (uint32_t)(((uint64_t)size + CFS5_BLOCK_SIZE - 1) /
                               CFS5_BLOCK_SIZE);
    if (blocks_needed) {
        blocks = malloc((size_t)blocks_needed * sizeof(*blocks));
        if (!blocks) {
            perror("malloc blocks");
            return false;
        }
    }

    while (allocated < blocks_needed) {
        uint32_t start_block;
        uint32_t run_count = allocate_block_run(
            sb, img, blocks_needed - allocated, &start_block);
        if (!run_count) {
            fprintf(stderr, "No free block available while replacing %s\n", path);
            goto rollback_new_blocks;
        }
        for (uint32_t i = 0; i < run_count; i++)
            blocks[allocated++] = start_block + i;
    }

    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint8_t block_buf[CFS5_BLOCK_SIZE] = {0};
        size_t offset = (size_t)i * CFS5_BLOCK_SIZE;
        size_t chunk = size - offset;
        if (chunk > CFS5_BLOCK_SIZE) chunk = CFS5_BLOCK_SIZE;
        memcpy(block_buf, data + offset, chunk);
        if (!write_block(img, blocks[i], block_buf)) {
            fprintf(stderr, "Failed writing replacement data for %s\n", path);
            goto rollback_new_blocks;
        }
    }

    new_inode = old_inode;
    new_inode.size = (uint32_t)size;
    new_inode.extent_count = 0;
    memset(new_inode.extents, 0, sizeof(new_inode.extents));
    for (uint32_t i = 0; i < blocks_needed; i++) {
        if (new_inode.extent_count &&
            (uint64_t)new_inode.extents[new_inode.extent_count - 1].start_block +
                new_inode.extents[new_inode.extent_count - 1].block_count == blocks[i]) {
            new_inode.extents[new_inode.extent_count - 1].block_count++;
        } else {
            if (new_inode.extent_count >= CFS5_MAX_EXTENTS) {
                fprintf(stderr, "Replacement for %s needs too many extents\n", path);
                goto rollback_new_blocks;
            }
            new_inode.extents[new_inode.extent_count].start_block = blocks[i];
            new_inode.extents[new_inode.extent_count].block_count = 1;
            new_inode.extent_count++;
        }
    }
    if (!write_inode(img, sb, file_ino, &new_inode)) {
        fprintf(stderr, "Failed writing replacement inode for %s\n", path);
        goto rollback_new_blocks;
    }

    for (uint32_t i = 0; i < old_inode.extent_count; i++) {
        for (uint32_t b = 0; b < old_inode.extents[i].block_count; b++) {
            if (!release_block(sb, img, old_inode.extents[i].start_block + b)) {
                fprintf(stderr, "Updated %s, but failed to release an old data block\n",
                        path);
                free(blocks);
                return false;
            }
        }
    }
    free(blocks);
    printf("Updated %s in inode %u\n", path, file_ino);
    return true;

rollback_new_blocks:
    while (allocated) {
        allocated--;
        if (!release_block(sb, img, blocks[allocated]))
            fprintf(stderr, "Failed to release temporary block %u\n",
                    blocks[allocated]);
    }
    free(blocks);
    return false;
}

/* Allocate a free inode from the bitmap */
static bool allocate_inode(const cfs5_superblock_t *sb, FILE* img, uint32_t* ino) {
    uint32_t bitmap_block = sb->free_inode_bitmap;
    uint8_t bitmap[CFS5_BLOCK_SIZE];

    if (!read_block(img, bitmap_block, bitmap)) {
        return false;
    }

    for (uint32_t i = 0; i < sb->inode_count; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            bitmap[byte_idx] |= (1 << bit_idx);
            if (!write_block(img, bitmap_block, bitmap)) {
                return false;
            }
            *ino = i;
            return true;
        }
    }
    return false; // No free inodes
}

/* Write an inode to the inode table */
static bool write_inode(FILE* img, const cfs5_superblock_t* sb, uint32_t ino, const cfs5_inode_t* in) {
    uint32_t inode_table_start = sb->inode_table_start;
    uint32_t inode_size = sizeof(cfs5_inode_t);
    uint32_t inodes_per_block = CFS5_BLOCK_SIZE / inode_size;
    uint32_t block_offset = ino / inodes_per_block;
    uint32_t offset_in_block = (ino % inodes_per_block) * inode_size;
    uint32_t block_num = inode_table_start + block_offset;

    // Allocate buffer on heap to avoid stack overflow with large inode counts
    cfs5_inode_t* buffer = malloc(CFS5_BLOCK_SIZE);
    if (!buffer) {
        return false;
    }

    if (!read_block(img, block_num, buffer)) {
        free(buffer);
        return false;
    }

    buffer[offset_in_block / sizeof(cfs5_inode_t)] = *in;

    if (!write_block(img, block_num, buffer)) {
        free(buffer);
        return false;
    }

    free(buffer);
    return true;
}

/* Read an inode from the inode table */
static bool read_inode(FILE* img, const cfs5_superblock_t* sb, uint32_t ino, cfs5_inode_t* in) {
    if (ino >= sb->inode_count) {
        return false;
    }
    uint32_t inode_table_start = sb->inode_table_start;
    uint32_t inode_size = sizeof(cfs5_inode_t);
    uint32_t inodes_per_block = CFS5_BLOCK_SIZE / inode_size;
    uint32_t block_offset = ino / inodes_per_block;
    uint32_t offset_in_block = (ino % inodes_per_block) * inode_size;
    uint32_t block_num = inode_table_start + block_offset;

    // Allocate buffer on heap to avoid stack overflow with large inode counts
    cfs5_inode_t* buffer = malloc(CFS5_BLOCK_SIZE);
    if (!buffer) {
        return false;
    }

    if (!read_block(img, block_num, buffer)) {
        free(buffer);
        return false;
    }
    *in = buffer[offset_in_block / sizeof(cfs5_inode_t)];
    free(buffer);
    return true;
}

/* Find or create a directory entry for a file in the directory (dir_ino).
 * If create is true and the entry doesn't exist, it will be created.
 * is_dir indicates whether the entry being created is a directory (true) or a file (false).
 * Returns the inode number of the file on success, 0 on failure.
 */
static uint32_t find_or_create_dirent(FILE* img, const cfs5_superblock_t* sb,
                                      uint32_t dir_ino, const char* name,
                                      bool create, bool is_dir)
{
    cfs5_inode_t dir_inode;
    uint8_t block_buf[CFS5_BLOCK_SIZE];
    uint32_t max_entries = CFS5_BLOCK_SIZE / sizeof(cfs5_dirent_t);

    if (!read_inode(img, sb, dir_ino, &dir_inode) ||
        dir_inode.type != CFS5_INODE_DIR ||
        dir_inode.extent_count > CFS5_MAX_EXTENTS)
        return 0;

    for (uint32_t extent_index = 0; extent_index < dir_inode.extent_count;
         extent_index++) {
        cfs5_extent_t* extent = &dir_inode.extents[extent_index];
        if (!extent->block_count || extent->start_block >= sb->block_count ||
            extent->block_count > sb->block_count - extent->start_block)
            return 0;
        for (uint32_t block_index = 0; block_index < extent->block_count;
             block_index++) {
            uint32_t block = extent->start_block + block_index;
            if (!read_block(img, block, block_buf)) return 0;
            cfs5_dirent_t* entries = (cfs5_dirent_t*)block_buf;
            for (uint32_t index = 0; index < max_entries; index++) {
                if (entries[index].inode && !strcmp(entries[index].name, name))
                    return entries[index].inode;
                if (!create || entries[index].inode) continue;

                uint32_t file_ino;
                if (!allocate_inode(sb, img, &file_ino)) return 0;
                cfs5_inode_t child;
                memset(&child, 0, sizeof(child));
                child.type = is_dir ? CFS5_INODE_DIR : CFS5_INODE_FILE;
                child.mode = 0755;
                if (!write_inode(img, sb, file_ino, &child)) return 0;
                entries[index].inode = file_ino;
                strncpy(entries[index].name, name, CFS5_MAX_NAME - 1);
                entries[index].name[CFS5_MAX_NAME - 1] = 0;
                if (!write_block(img, block, block_buf)) return 0;
                dir_inode.size += sizeof(cfs5_dirent_t);
                if (!write_inode(img, sb, dir_ino, &dir_inode)) return 0;
                return file_ino;
            }
        }
    }

    if (!create || dir_inode.extent_count >= CFS5_MAX_EXTENTS) return 0;

    uint32_t block = allocate_block(sb, img);
    uint32_t file_ino;
    if (!block || !allocate_inode(sb, img, &file_ino)) return 0;
    memset(block_buf, 0, sizeof(block_buf));
    cfs5_dirent_t* first_entry = (cfs5_dirent_t*)block_buf;
    first_entry[0].inode = file_ino;
    strncpy(first_entry[0].name, name, CFS5_MAX_NAME - 1);
    first_entry[0].name[CFS5_MAX_NAME - 1] = 0;
    if (!write_block(img, block, block_buf)) return 0;

    cfs5_inode_t child;
    memset(&child, 0, sizeof(child));
    child.type = is_dir ? CFS5_INODE_DIR : CFS5_INODE_FILE;
    child.mode = 0755;
    if (!write_inode(img, sb, file_ino, &child)) return 0;

    if (dir_inode.extent_count) {
        cfs5_extent_t* last = &dir_inode.extents[dir_inode.extent_count - 1];
        if ((uint64_t)last->start_block + last->block_count == block) {
            last->block_count++;
        } else {
            dir_inode.extents[dir_inode.extent_count].start_block = block;
            dir_inode.extents[dir_inode.extent_count].block_count = 1;
            dir_inode.extent_count++;
        }
    } else {
        dir_inode.extents[0].start_block = block;
        dir_inode.extents[0].block_count = 1;
        dir_inode.extent_count = 1;
    }
    dir_inode.size += sizeof(cfs5_dirent_t);
    if (!write_inode(img, sb, dir_ino, &dir_inode)) return 0;
    return file_ino;
}

/* Create a directory path (including parent directories) in the image.
 * Returns the inode of the final directory on success, 0 on failure.
 */
static uint32_t create_directory_path(FILE* img, const cfs5_superblock_t* sb, const char* path) {
    // Skip leading slash
    const char* start = path;
    if (*start == '/') {
        start++;
    }

    // Start at the root directory from superblock
    uint32_t current_ino = sb->root_inode;

    // Temporary buffer for each component
    char* path_copy = strdup(path);
    if (!path_copy) {
        return 0;
    }
    char* saveptr;
    char* token = strtok_r(path_copy, "/", &saveptr);

    while (token != NULL) {
        // Find or create the directory entry for this component (it's a directory)
        uint32_t next_ino = find_or_create_dirent(img, sb, current_ino, token, true, true);
        if (next_ino == 0) {
            fprintf(stderr, "DEBUG: create_directory_path: failed to create entry for '%s' in directory %u\n", token, current_ino);
            free(path_copy);
            return 0;
        }
        current_ino = next_ino;
        token = strtok_r(NULL, "/", &saveptr);
    }

    free(path_copy);
    return current_ino;
}

/* Recursively copy the contents of a source directory to a directory inode in the image */
static int copy_directory_contents(FILE* img, const cfs5_superblock_t* sb, const char* src_path, uint32_t dest_dir_ino) {
    DIR* dir = opendir(src_path);
    if (!dir) {
        fprintf(stderr, "Failed to open directory %s: %s\n", src_path, strerror(errno));
        return 1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full source path
        char src_full[PATH_MAX];
        snprintf(src_full, sizeof(src_full), "%s/%s", src_path, entry->d_name);

        struct stat st;
        if (stat(src_full, &st) == -1) {
            fprintf(stderr, "Failed to stat %s: %s\n", src_full, strerror(errno));
            closedir(dir);
            return 1;
        }

        if (S_ISDIR(st.st_mode)) {
            // Create the subdirectory in the image
            uint32_t subdir_ino = find_or_create_dirent(img, sb, dest_dir_ino, entry->d_name, true, true);
            if (subdir_ino == 0) {
                fprintf(stderr, "Failed to create directory %s in image\n", entry->d_name);
                closedir(dir);
                return 1;
            }
            // Recursively copy the subdirectory
            if (copy_directory_contents(img, sb, src_full, subdir_ino) != 0) {
                closedir(dir);
                return 1;
            }
        } else if (S_ISREG(st.st_mode)) {
            // Copy the file
            FILE* src = fopen(src_full, "rb");
            if (!src) {
                fprintf(stderr, "Failed to open %s: %s\n", src_full, strerror(errno));
                closedir(dir);
                return 1;
            }
            fseek(src, 0, SEEK_END);
            long src_size = ftell(src);
            if (src_size < 0 || (uint64_t)src_size > UINT32_MAX) {
                fprintf(stderr, "Invalid source file size for %s\n", src_full);
                fclose(src);
                closedir(dir);
                return 1;
            }
            fseek(src, 0, SEEK_SET);
            uint8_t* src_data = malloc(src_size ? (size_t)src_size : 1);
            if (!src_data) {
                perror("malloc");
                fclose(src);
                closedir(dir);
                return 1;
            }
            if (fread(src_data, 1, (size_t)src_size, src) != (size_t)src_size) {
                perror("fread");
                free(src_data);
                fclose(src);
                closedir(dir);
                return 1;
            }
            fclose(src);

            // Find or create the directory entry for the file
            uint32_t file_ino = find_or_create_dirent(img, sb, dest_dir_ino, entry->d_name, true, false);
            if (file_ino == 0) {
                fprintf(stderr, "Failed to create directory entry for %s\n", entry->d_name);
                free(src_data);
                closedir(dir);
                return 1;
            }

            if (!replace_file_contents(img, sb, file_ino, src_data,
                                       (size_t)src_size, src_full)) {
                free(src_data);
                closedir(dir);
                return 1;
            }

            free(src_data);
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char** argv) {
    printf("Starting copy_to_cfsley\n");
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <image> <source_path> <dest_path_in_image>\n", argv[0]);
        return 1;
    }

    const char* img_path = argv[1];
    const char* src_path = argv[2];
    const char* dest_path = argv[3];

    FILE* img = fopen(img_path, "r+b");
    if (!img) {
        perror("fopen");
        return 1;
    }

    cfs5_superblock_t sb;
    if (!read_superblock(img, &sb)) {
        fprintf(stderr, "Failed to read superblock\n");
        fclose(img);
        return 1;
    }

    printf("DEBUG: superblock: magic=%x, version=%d, block_size=%d, block_count=%u\n",
           sb.magic, sb.version, sb.block_size, sb.block_count);
    printf("DEBUG: inode_count=%u, inode_table_start=%u, inode_table_blocks=%u\n",
           sb.inode_count, sb.inode_table_start, sb.inode_table_blocks);
    printf("DEBUG: free_block_bitmap=%u, free_block_bitmap_blocks=%u\n",
           sb.free_block_bitmap, sb.free_block_bitmap_blocks);
    printf("DEBUG: free_inode_bitmap=%u, free_inode_bitmap_blocks=%u\n",
           sb.free_inode_bitmap, sb.free_inode_bitmap_blocks);
    printf("DEBUG: data_block_start=%u\n", sb.data_block_start);
    printf("DEBUG: root_inode=%u\n", sb.root_inode);

    struct stat src_stat;
    if (stat(src_path, &src_stat) == -1) {
        fprintf(stderr, "Failed to stat %s: %s\n", src_path, strerror(errno));
        fclose(img);
        return 1;
    }

    if (S_ISDIR(src_stat.st_mode)) {
        // Make a copy of dest_path and remove trailing slashes
        char* dest_path_copy = strdup(dest_path);
        if (!dest_path_copy) {
            perror("strdup");
            fclose(img);
            return 1;
        }
        // Remove trailing slashes
        int len = strlen(dest_path_copy);
        while (len > 1 && dest_path_copy[len-1] == '/') {
            dest_path_copy[--len] = '\0';
        }
        // Now check if it's "/"
        uint32_t dest_dir_ino;
        if (strcmp(dest_path_copy, "/") == 0) {
            dest_dir_ino = sb.root_inode;
        } else {
            dest_dir_ino = create_directory_path(img, &sb, dest_path_copy);
            if (dest_dir_ino == 0) {
                fprintf(stderr, "Failed to create destination directory path: %s\n", dest_path_copy);
                free(dest_path_copy);
                fclose(img);
                return 1;
            }
        }
        free(dest_path_copy);

        // Copy the contents of the source directory to the destination directory
        if (copy_directory_contents(img, &sb, src_path, dest_dir_ino) != 0) {
            fclose(img);
            return 1;
        }
    } else if (S_ISREG(src_stat.st_mode)) {
        // For a file, we need to split the destination path into directory and filename
        char* dest_path_copy = strdup(dest_path);
        if (!dest_path_copy) {
            perror("strdup");
            fclose(img);
            return 1;
        }
        // Remove trailing slashes
        int len = strlen(dest_path_copy);
        while (len > 1 && dest_path_copy[len-1] == '/') {
            dest_path_copy[--len] = '\0';
        }
        char* dir_part = dirname(dest_path_copy);
        char* file_part = basename(dest_path_copy);

        uint32_t dir_ino;
        if (strcmp(dir_part, ".") == 0 || strcmp(dir_part, "/") == 0) {
            // Destination is in the root directory
            dir_ino = sb.root_inode;
        } else {
            dir_ino = create_directory_path(img, &sb, dir_part);
            if (dir_ino == 0) {
                fprintf(stderr, "Failed to create directory path for file: %s\n", dir_part);
                free(dest_path_copy);
                fclose(img);
                return 1;
            }
        }

        // Read the source file
        FILE* src = fopen(src_path, "rb");
        if (!src) {
            perror("fopen source");
            free(dest_path_copy);
            fclose(img);
            return 1;
        }
        fseek(src, 0, SEEK_END);
        long src_size = ftell(src);
        if (src_size < 0 || (uint64_t)src_size > UINT32_MAX) {
            fprintf(stderr, "Invalid source file size for %s\n", src_path);
            fclose(src);
            free(dest_path_copy);
            fclose(img);
            return 1;
        }
        fseek(src, 0, SEEK_SET);
        uint8_t* src_data = malloc(src_size ? (size_t)src_size : 1);
        if (!src_data) {
            perror("malloc");
            fclose(src);
            free(dest_path_copy);
            fclose(img);
            return 1;
        }
        if (fread(src_data, 1, (size_t)src_size, src) != (size_t)src_size) {
            perror("fread");
            free(src_data);
            fclose(src);
            free(dest_path_copy);
            fclose(img);
            return 1;
        }
        fclose(src);

        // Find or create the directory entry for the file in the destination directory
        uint32_t file_ino = find_or_create_dirent(img, &sb, dir_ino, file_part, true, false);
        if (file_ino == 0) {
            fprintf(stderr, "Failed to create directory entry for %s\n", file_part);
            free(src_data);
            free(dest_path_copy);
            fclose(img);
            return 1;
        }

        if (!replace_file_contents(img, &sb, file_ino, src_data,
                                   (size_t)src_size, dest_path)) {
            free(src_data);
            free(dest_path_copy);
            fclose(img);
            return 1;
        }
        free(src_data);
        free(dest_path_copy);
    } else {
        fprintf(stderr, "Unsupported file type for %s\n", src_path);
        fclose(img);
        return 1;
    }

    fclose(img);
    return 0;
}
