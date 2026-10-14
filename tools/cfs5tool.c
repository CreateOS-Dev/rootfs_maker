#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

#include "cfs5.h"

typedef struct {
    FILE* file;
    cfs5_superblock_t super;
    uint64_t image_size;
    const char* source_path;
} cfs5_image_t;

static void usage(FILE* stream, const char* program)
{
    fprintf(stream,
            "Usage:\n"
            "  %s info IMAGE\n"
            "  %s ls IMAGE [PATH]\n"
            "  %s check IMAGE\n"
            "  %s probe IMAGE PATH\n"
            "  %s extract IMAGE PATH OUTPUT [--force]\n",
            program, program, program, program, program);
}

static int read_at(FILE* file, uint64_t offset, void* data, size_t size)
{
    if (offset > (uint64_t)LONG_MAX ||
        fseek(file, (long)offset, SEEK_SET) != 0)
        return 0;
    return fread(data, 1, size, file) == size;
}

static int read_block(cfs5_image_t* image, uint32_t block, void* data)
{
    uint64_t offset;
    if (!image || block >= image->super.block_count) return 0;
    offset = (uint64_t)block * CFS5_BLOCK_SIZE;
    if (offset + CFS5_BLOCK_SIZE > image->image_size) return 0;
    return read_at(image->file, offset, data, CFS5_BLOCK_SIZE);
}

static int super_valid(const cfs5_superblock_t* super, uint64_t image_size)
{
    uint64_t inode_table_end, block_bitmap_end, inode_bitmap_end;
    uint64_t required_inode_blocks;
    uint64_t inode_capacity, block_capacity;
    uint32_t inodes_per_block = CFS5_BLOCK_SIZE / sizeof(cfs5_inode_t);

    if (!super || !inodes_per_block ||
        super->magic != CFS5_MAGIC || super->version != CFS5_VERSION ||
        super->block_size != CFS5_BLOCK_SIZE || !super->block_count ||
        !super->inode_count || super->root_inode >= super->inode_count ||
        (uint64_t)super->block_count * CFS5_BLOCK_SIZE > image_size)
        return 0;
    required_inode_blocks =
        ((uint64_t)super->inode_count + inodes_per_block - 1) /
        inodes_per_block;
    if (!super->inode_table_start ||
        super->inode_table_blocks < required_inode_blocks ||
        super->inode_table_start >= super->block_count ||
        super->inode_table_blocks >
            super->block_count - super->inode_table_start ||
        !super->free_block_bitmap_blocks ||
        super->free_block_bitmap >= super->block_count ||
        super->free_block_bitmap_blocks >
            super->block_count - super->free_block_bitmap ||
        !super->free_inode_bitmap_blocks ||
        super->free_inode_bitmap >= super->block_count ||
        super->free_inode_bitmap_blocks >
            super->block_count - super->free_inode_bitmap)
        return 0;

    inode_table_end = (uint64_t)super->inode_table_start +
                      super->inode_table_blocks;
    block_bitmap_end = (uint64_t)super->free_block_bitmap +
                       super->free_block_bitmap_blocks;
    inode_bitmap_end = (uint64_t)super->free_inode_bitmap +
                       super->free_inode_bitmap_blocks;
    if (super->free_block_bitmap < inode_table_end ||
        super->free_inode_bitmap < block_bitmap_end ||
        super->data_block_start < inode_bitmap_end ||
        super->data_block_start >= super->block_count)
        return 0;
    inode_capacity = (uint64_t)super->free_inode_bitmap_blocks *
                     CFS5_BLOCK_SIZE * 8;
    block_capacity = (uint64_t)super->free_block_bitmap_blocks *
                     CFS5_BLOCK_SIZE * 8;
    if (inode_capacity < super->inode_count ||
        block_capacity < super->block_count - super->data_block_start)
        return 0;
    if ((super->journal_blocks &&
         (super->journal_start >= super->block_count ||
          super->journal_blocks >
              super->block_count - super->journal_start)) ||
        (super->snapshot_meta_blocks &&
         (super->snapshot_meta_start >= super->block_count ||
          super->snapshot_meta_blocks >
              super->block_count - super->snapshot_meta_start)))
        return 0;
    return 1;
}

static int image_open(cfs5_image_t* image, const char* path)
{
    long file_size;
    if (!image || !path) return 0;
    memset(image, 0, sizeof(*image));
    image->source_path = path;
    image->file = fopen(path, "rb");
    if (!image->file) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return 0;
    }
    if (fseek(image->file, 0, SEEK_END) != 0 ||
        (file_size = ftell(image->file)) < 0 ||
        !read_at(image->file, 0, &image->super, sizeof(image->super))) {
        fprintf(stderr, "%s: cannot read image metadata\n", path);
        fclose(image->file);
        image->file = NULL;
        return 0;
    }
    image->image_size = (uint64_t)file_size;
    if (!super_valid(&image->super, image->image_size)) {
        fprintf(stderr, "%s: invalid or unsupported CFS5 image\n", path);
        fclose(image->file);
        image->file = NULL;
        return 0;
    }
    return 1;
}

static void image_close(cfs5_image_t* image)
{
    if (image && image->file) fclose(image->file);
    if (image) image->file = NULL;
}

static int read_inode(cfs5_image_t* image, uint32_t number,
                      cfs5_inode_t* inode)
{
    uint32_t per_block = CFS5_BLOCK_SIZE / sizeof(*inode);
    uint32_t block;
    uint32_t index;
    uint8_t buffer[CFS5_BLOCK_SIZE];
    if (!image || !inode || number >= image->super.inode_count) return 0;
    block = image->super.inode_table_start + number / per_block;
    index = number % per_block;
    if (!read_block(image, block, buffer)) return 0;
    memcpy(inode, buffer + index * sizeof(*inode), sizeof(*inode));
    return 1;
}

static int inode_allocated(cfs5_image_t* image, uint32_t number)
{
    uint32_t bits_per_block = CFS5_BLOCK_SIZE * 8U;
    uint32_t block_offset = number / bits_per_block;
    uint32_t bit = number % bits_per_block;
    uint8_t buffer[CFS5_BLOCK_SIZE];
    if (block_offset >= image->super.free_inode_bitmap_blocks ||
        !read_block(image, image->super.free_inode_bitmap + block_offset,
                    buffer))
        return -1;
    return !!(buffer[bit / 8] & (1U << (bit % 8)));
}

static int inode_valid(cfs5_image_t* image, const cfs5_inode_t* inode)
{
    uint64_t capacity = 0;
    if (!inode || inode->extent_count > CFS5_MAX_EXTENTS ||
        (inode->type != CFS5_INODE_FILE && inode->type != CFS5_INODE_DIR &&
         inode->type != CFS5_INODE_SYMLINK) ||
        (inode->flags & ~(CFS5_IF_COMPRESSED | CFS5_IF_IMMUTABLE |
                          CFS5_IF_APPENDONLY)))
        return 0;
    for (uint32_t i = 0; i < inode->extent_count; i++) {
        const cfs5_extent_t* extent = &inode->extents[i];
        if (!extent->block_count ||
            extent->start_block < image->super.data_block_start ||
            extent->start_block >= image->super.block_count ||
            extent->block_count >
                image->super.block_count - extent->start_block)
            return 0;
        capacity += (uint64_t)extent->block_count * CFS5_BLOCK_SIZE;
    }
    return inode->size <= capacity;
}

static int name_valid(const char name[CFS5_MAX_NAME])
{
    for (size_t i = 0; i < CFS5_MAX_NAME; i++) {
        if (!name[i]) return i != 0;
    }
    return 0;
}

static int resolve_path(cfs5_image_t* image, const char* path,
                        uint32_t* inode_number)
{
    char copy[512];
    char* token;
    char* save = NULL;
    uint32_t parents[128];
    size_t depth = 0;
    uint32_t current;
    if (!path || path[0] != '/' || strlen(path) >= sizeof(copy)) return 0;
    strcpy(copy, path);
    current = image->super.root_inode;
    token = strtok_r(copy, "/", &save);
    while (token) {
        cfs5_inode_t directory;
        uint32_t parent_inode = current;
        int found = 0;
        uint8_t block[CFS5_BLOCK_SIZE];
        if (!strcmp(token, ".")) {
            token = strtok_r(NULL, "/", &save);
            continue;
        }
        if (!strcmp(token, "..")) {
            if (depth) current = parents[--depth];
            token = strtok_r(NULL, "/", &save);
            continue;
        }
        if (depth == sizeof(parents) / sizeof(parents[0])) return 0;
        if (!read_inode(image, current, &directory) ||
            !inode_valid(image, &directory) ||
            directory.type != CFS5_INODE_DIR)
            return 0;
        for (uint32_t e = 0; e < directory.extent_count && !found; e++) {
            for (uint32_t b = 0; b < directory.extents[e].block_count && !found;
                 b++) {
                if (!read_block(image, directory.extents[e].start_block + b,
                                block))
                    return 0;
                cfs5_dirent_t* entries = (cfs5_dirent_t*)block;
                size_t count = CFS5_BLOCK_SIZE / sizeof(*entries);
                for (size_t i = 0; i < count; i++) {
                    if (!entries[i].inode ||
                        !name_valid(entries[i].name))
                        continue;
                    if (entries[i].inode >= image->super.inode_count) return 0;
                    if (!strcmp(entries[i].name, token)) {
                        current = entries[i].inode;
                        found = 1;
                        break;
                    }
                }
            }
        }
        if (!found) return 0;
        parents[depth++] = parent_inode;
        token = strtok_r(NULL, "/", &save);
    }
    *inode_number = current;
    return 1;
}

static void print_mode(const cfs5_inode_t* inode)
{
    static const uint16_t bits[] = { 0400, 0200, 0100, 0040, 0020,
                                     0010, 0004, 0002, 0001 };
    static const char letters[] = "rwxrwxrwx";
    putchar(inode->type == CFS5_INODE_DIR ? 'd' : '-');
    for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
        putchar(inode->mode & bits[i] ? letters[i] : '-');
}

static int command_info(const char* path)
{
    FILE* file = fopen(path, "rb");
    uint8_t header[512] = { 0 };
    cfs5_superblock_t super;
    long size;
    if (!file) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return 1;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        !read_at(file, 0, header, sizeof(header))) {
        fprintf(stderr, "%s: cannot read image\n", path);
        fclose(file);
        return 1;
    }
    memcpy(&super, header, sizeof(super));
    printf("Image: %s\nSize: %llu bytes\n",
           path, (unsigned long long)size);
    if (super_valid(&super, (uint64_t)size)) {
        printf("Format: CFS5 v%u\nBlock size: %u\nBlocks: %u\n"
               "Inodes: %u\nRoot inode: %u\nData starts: block %u\n"
               "Journal blocks: %u\nSnapshot metadata blocks: %u\n",
               super.version, super.block_size, super.block_count,
               super.inode_count, super.root_inode, super.data_block_start,
               super.journal_blocks, super.snapshot_meta_blocks);
        printf("Encryption: no encrypted flag is defined by this CFS5 version\n");
    } else if (size >= 6 && memcmp(header, "LUKS", 4) == 0 &&
               header[4] == 0xBA && header[5] == 0xBE) {
        puts("Format: LUKS encrypted-volume container signature detected");
    } else if (size >= 512 &&
               memcmp(header + 3, "-FVE-FS-", 8) == 0) {
        puts("Format: BitLocker volume signature detected");
    } else if (size >= 40 && memcmp(header + 32, "NXSB", 4) == 0) {
        puts("Format: APFS container superblock signature detected");
    } else if (size >= 512 && header[510] == 0x55 &&
               header[511] == 0xAA) {
        if (memcmp(header + 82, "FAT32   ", 8) == 0)
            puts("Format: FAT32 boot sector signature detected");
        else
            puts("Format: MBR-style boot signature detected; partition type not fully validated");
    } else {
        puts("Format: unknown or unsupported");
    }
    fclose(file);
    return 0;
}

static int command_ls(cfs5_image_t* image, const char* path)
{
    uint32_t number;
    cfs5_inode_t directory;
    uint8_t block[CFS5_BLOCK_SIZE];
    uint64_t remaining;
    if (!resolve_path(image, path, &number) ||
        !read_inode(image, number, &directory) ||
        !inode_valid(image, &directory) ||
        directory.type != CFS5_INODE_DIR ||
        directory.size % sizeof(cfs5_dirent_t)) {
        fprintf(stderr, "Cannot resolve or list CFS5 directory: %s\n", path);
        return 1;
    }
    printf("Directory: %s (inode %u, %u bytes)\n",
           path, number, directory.size);
    remaining = directory.size / sizeof(cfs5_dirent_t);
    for (uint32_t e = 0; e < directory.extent_count; e++) {
        for (uint32_t b = 0; b < directory.extents[e].block_count; b++) {
            if (!read_block(image, directory.extents[e].start_block + b,
                            block)) {
                fprintf(stderr, "Cannot read directory block\n");
                return 1;
            }
            cfs5_dirent_t* entries = (cfs5_dirent_t*)block;
            size_t count = CFS5_BLOCK_SIZE / sizeof(*entries);
            for (size_t i = 0; i < count; i++) {
                cfs5_inode_t child;
                if (!entries[i].inode) continue;
                if (!remaining || !name_valid(entries[i].name) ||
                    !read_inode(image, entries[i].inode, &child) ||
                    !inode_valid(image, &child)) {
                    fprintf(stderr, "Malformed directory entry or inode\n");
                    return 1;
                }
                remaining--;
                print_mode(&child);
                printf(" %u %u %u %s\n", child.uid, child.gid, child.size,
                       entries[i].name);
            }
        }
    }
    if (remaining) {
        fprintf(stderr, "Directory size does not match its entries\n");
        return 1;
    }
    return 0;
}

static int check_directory(cfs5_image_t* image, uint32_t number,
                           const cfs5_inode_t* inode, uint8_t* allocated)
{
    uint64_t entries_left;
    uint8_t block[CFS5_BLOCK_SIZE];
    if (inode->type != CFS5_INODE_DIR) return 1;
    if (inode->size % sizeof(cfs5_dirent_t)) return 0;
    entries_left = inode->size / sizeof(cfs5_dirent_t);
    for (uint32_t e = 0; e < inode->extent_count; e++) {
        for (uint32_t b = 0; b < inode->extents[e].block_count; b++) {
            if (!read_block(image, inode->extents[e].start_block + b, block))
                return 0;
            cfs5_dirent_t* entries = (cfs5_dirent_t*)block;
            size_t count = CFS5_BLOCK_SIZE / sizeof(*entries);
            for (size_t i = 0; i < count; i++) {
                if (!entries[i].inode) continue;
                if (!entries_left || !name_valid(entries[i].name) ||
                    entries[i].inode >= image->super.inode_count ||
                    !allocated[entries[i].inode])
                    return 0;
                entries_left--;
            }
        }
    }
    (void)number;
    return entries_left == 0;
}

static int command_check(cfs5_image_t* image)
{
    uint8_t* allocated =
        (uint8_t*)calloc(image->super.inode_count, sizeof(*allocated));
    uint8_t* used_blocks =
        (uint8_t*)calloc(image->super.block_count, sizeof(*used_blocks));
    uint32_t inode_total = 0;
    uint32_t directory_total = 0;
    uint32_t file_total = 0;
    int errors = 0;

    if (!allocated || !used_blocks) {
        fprintf(stderr, "Not enough memory for filesystem check\n");
        free(allocated);
        free(used_blocks);
        return 1;
    }
    for (uint32_t i = 1; i < image->super.inode_count; i++) {
        int used = inode_allocated(image, i);
        if (used < 0) {
            errors++;
            break;
        }
        allocated[i] = (uint8_t)used;
    }
    if (!allocated[image->super.root_inode]) {
        fprintf(stderr, "Root inode is marked free\n");
        errors++;
    }
    for (uint32_t i = 1; i < image->super.inode_count; i++) {
        cfs5_inode_t inode;
        if (!allocated[i]) continue;
        inode_total++;
        if (!read_inode(image, i, &inode) || !inode_valid(image, &inode)) {
            fprintf(stderr, "Invalid inode %u\n", i);
            errors++;
            continue;
        }
        if (inode.type == CFS5_INODE_DIR) directory_total++;
        else if (inode.type == CFS5_INODE_FILE) file_total++;
        if (!check_directory(image, i, &inode, allocated)) {
            fprintf(stderr, "Invalid directory entries in inode %u\n", i);
            errors++;
        }
        for (uint32_t e = 0; e < inode.extent_count; e++) {
            for (uint32_t b = 0; b < inode.extents[e].block_count; b++) {
                uint32_t block = inode.extents[e].start_block + b;
                if (used_blocks[block]) {
                    fprintf(stderr, "Block %u is referenced more than once\n",
                            block);
                    errors++;
                } else {
                    used_blocks[block] = 1;
                }
            }
        }
    }
    if (errors)
        fprintf(stderr, "CFS5 check failed: %d issue(s) found\n", errors);
    else
        printf("CFS5 check passed: %u allocated inodes (%u directories, "
               "%u files); no structural errors found\n",
               inode_total, directory_total, file_total);
    free(allocated);
    free(used_blocks);
    return errors ? 1 : 0;
}

static int read_file_data(cfs5_image_t* image, const cfs5_inode_t* inode,
                          FILE* output)
{
    uint8_t block[CFS5_BLOCK_SIZE];
    uint32_t remaining = inode->size;
    for (uint32_t e = 0; e < inode->extent_count && remaining; e++) {
        for (uint32_t b = 0; b < inode->extents[e].block_count && remaining;
             b++) {
            size_t amount = remaining > CFS5_BLOCK_SIZE ?
                            CFS5_BLOCK_SIZE : remaining;
            if (!read_block(image, inode->extents[e].start_block + b, block) ||
                fwrite(block, 1, amount, output) != amount)
                return 0;
            remaining -= (uint32_t)amount;
        }
    }
    return remaining == 0;
}

static int command_probe(cfs5_image_t* image, const char* path)
{
    uint32_t number;
    cfs5_inode_t inode;
    uint8_t buffer[65536];
    uint8_t block[CFS5_BLOCK_SIZE];
    uint64_t frequencies[256] = { 0 };
    uint32_t sample_size = 0;
    uint32_t remaining;
    double entropy = 0.0;

    if (!resolve_path(image, path, &number) ||
        !read_inode(image, number, &inode) ||
        !inode_valid(image, &inode) || inode.type != CFS5_INODE_FILE) {
        fprintf(stderr, "Cannot read regular file: %s\n", path);
        return 1;
    }
    printf("File: %s\nSize: %u bytes\nCFS5 flags: 0x%04x\n",
           path, inode.size, inode.flags);
    if (inode.flags & CFS5_IF_COMPRESSED)
        puts("Compression: metadata flag set");
    else
        puts("Compression: not indicated by metadata");
    puts("Encryption: this CFS5 version has no encrypted-file flag; encryption cannot be confirmed from bytes alone");

    remaining = inode.size > sizeof(buffer) ? sizeof(buffer) : inode.size;
    for (uint32_t e = 0; e < inode.extent_count && sample_size < remaining; e++) {
        for (uint32_t b = 0;
             b < inode.extents[e].block_count && sample_size < remaining; b++) {
            uint32_t amount = remaining - sample_size;
            if (amount > CFS5_BLOCK_SIZE) amount = CFS5_BLOCK_SIZE;
            if (!read_block(image, inode.extents[e].start_block + b, block)) {
                fprintf(stderr, "Cannot sample file data\n");
                return 1;
            }
            memcpy(buffer + sample_size, block, amount);
            sample_size += amount;
        }
    }
    for (uint32_t i = 0; i < sample_size; i++)
        frequencies[buffer[i]]++;
    if (sample_size) {
        for (size_t i = 0; i < 256; i++) {
            if (frequencies[i]) {
                double probability =
                    (double)frequencies[i] / (double)sample_size;
                entropy -= probability * log2(probability);
            }
        }
    }
    printf("Sample: %u bytes; Shannon entropy: %.3f bits/byte\n",
           sample_size, entropy);
    if (sample_size && entropy >= 7.5)
        puts("Heuristic: high-entropy data (may be encrypted, compressed, or random); this is not proof of encryption");
    else if (sample_size)
        puts("Heuristic: sample is not high entropy; this does not rule out encryption");
    return 0;
}

static int command_extract(cfs5_image_t* image, const char* path,
                           const char* output_path, int force)
{
    uint32_t number;
    cfs5_inode_t inode;
    struct stat input_stat, output_stat;
    FILE* output;
    int output_exists = stat(output_path, &output_stat) == 0;

    if (!resolve_path(image, path, &number) ||
        !read_inode(image, number, &inode) ||
        !inode_valid(image, &inode) || inode.type != CFS5_INODE_FILE) {
        fprintf(stderr, "Cannot extract regular file: %s\n", path);
        return 1;
    }
    if (inode.flags & CFS5_IF_COMPRESSED) {
        fprintf(stderr, "Cannot extract compressed CFS5 data: no decoder is available\n");
        return 1;
    }
    if (stat(image->source_path, &input_stat) == 0 &&
        stat(output_path, &output_stat) == 0 &&
        input_stat.st_dev == output_stat.st_dev &&
        input_stat.st_ino == output_stat.st_ino) {
        fprintf(stderr, "Refusing to overwrite the source image\n");
        return 1;
    }
    if (output_exists && !force) {
        fprintf(stderr, "%s already exists (use --force to replace it)\n",
                output_path);
        return 1;
    }
    output = fopen(output_path, "wb");
    if (!output) {
        fprintf(stderr, "%s: %s\n", output_path, strerror(errno));
        return 1;
    }
    int write_ok = read_file_data(image, &inode, output);
    int close_ok = fclose(output) == 0;
    if (!write_ok || !close_ok) {
        fprintf(stderr, "Extraction failed for %s\n", path);
        remove(output_path);
        return 1;
    }
    printf("Extracted %s to %s (%u bytes)\n",
           path, output_path, inode.size);
    return 0;
}

int main(int argc, char** argv)
{
    cfs5_image_t image;
    const char* path;
    int result;

    if (argc == 2 &&
        (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        usage(stdout, argv[0]);
        return 0;
    }
    if (argc < 3) {
        usage(stderr, argv[0]);
        return 2;
    }
    if (!strcmp(argv[1], "info")) {
        if (argc != 3) {
            usage(stderr, argv[0]);
            return 2;
        }
        return command_info(argv[2]);
    }
    if (!strcmp(argv[1], "extract")) {
        int force = argc == 6 && !strcmp(argv[5], "--force");
        if (argc != 5 && !force) {
            usage(stderr, argv[0]);
            return 2;
        }
        path = argv[3];
    } else if (!strcmp(argv[1], "ls") || !strcmp(argv[1], "check") ||
               !strcmp(argv[1], "probe")) {
        if ((!strcmp(argv[1], "check") && argc != 3) ||
            (!strcmp(argv[1], "ls") && (argc < 3 || argc > 4)) ||
            (!strcmp(argv[1], "probe") && argc != 4)) {
            usage(stderr, argv[0]);
            return 2;
        }
        path = argc >= 4 ? argv[3] : "/";
    } else {
        usage(stderr, argv[0]);
        return 2;
    }

    if (!image_open(&image, argv[2])) return 1;
    if (!strcmp(argv[1], "ls"))
        result = command_ls(&image, path);
    else if (!strcmp(argv[1], "check"))
        result = command_check(&image);
    else if (!strcmp(argv[1], "probe"))
        result = command_probe(&image, path);
    else
        result = command_extract(&image, path, argv[4],
                                 argc == 6 && !strcmp(argv[5], "--force"));
    image_close(&image);
    return result;
}
