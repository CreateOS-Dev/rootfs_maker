// ============================================================
// CreateOS 0.0.1
// ============================================================

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "cfs5.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <image> <size_in_blocks>\n", argv[0]);
        return 1;
    }

    const char* path = argv[1];
    uint32_t blocks = (uint32_t)strtoul(argv[2], NULL, 10);

    FILE* f = fopen(path, "wb+");
    if (!f) {
        perror("fopen");
        return 1;
    }

    uint8_t zero[CFS5_BLOCK_SIZE];
    memset(zero, 0, sizeof(zero));

    for (uint32_t i = 0; i < blocks; i++) {
        if (fwrite(zero, 1, CFS5_BLOCK_SIZE, f) != CFS5_BLOCK_SIZE) {
            perror("fwrite");
            fclose(f);
            return 1;
        }
    }

    cfs5_superblock_t sb;
    memset(&sb, 0, sizeof(sb));

    sb.magic       = CFS5_MAGIC;
    sb.version     = CFS5_VERSION;
    sb.block_size  = CFS5_BLOCK_SIZE;
    sb.block_count = blocks;

    sb.inode_count = 1024;
    sb.inode_table_start  = 1;
    sb.inode_table_blocks = (sb.inode_count * sizeof(cfs5_inode_t) + CFS5_BLOCK_SIZE - 1) / CFS5_BLOCK_SIZE;

    sb.free_block_bitmap = sb.inode_table_start + sb.inode_table_blocks;
    sb.free_block_bitmap_blocks = 1;

    sb.free_inode_bitmap = sb.free_block_bitmap + sb.free_block_bitmap_blocks;
    sb.free_inode_bitmap_blocks = 1;

    sb.data_block_start = sb.free_inode_bitmap + sb.free_inode_bitmap_blocks;

    sb.journal_start  = sb.data_block_start + 0;
    sb.journal_blocks = 0;

    sb.snapshot_meta_start  = sb.journal_start + sb.journal_blocks;
    sb.snapshot_meta_blocks = 0;

    sb.root_inode = 1;

    // Write superblock
    fseek(f, 0, SEEK_SET);
    fwrite(&sb, 1, sizeof(sb), f);

    // Initialize inode and block bitmaps
    uint8_t ibm[CFS5_BLOCK_SIZE];
    uint8_t bbm[CFS5_BLOCK_SIZE];
    memset(ibm, 0, sizeof(ibm));
    memset(bbm, 0, sizeof(bbm));

    // Mark inodes 0 and 1 as used (0 = null, 1 = root)
    ibm[0] |= 0x03; // inodes 0 and 1 used

    // Write inode bitmap
    fseek(f, sb.free_inode_bitmap * CFS5_BLOCK_SIZE, SEEK_SET);
    fwrite(ibm, 1, CFS5_BLOCK_SIZE, f);

    // Write block bitmap (all free for now)
    fseek(f, sb.free_block_bitmap * CFS5_BLOCK_SIZE, SEEK_SET);
    fwrite(bbm, 1, CFS5_BLOCK_SIZE, f);

    // Now initialize the root directory (inode 1)
    // 1. Allocate a block for the root directory data
    uint32_t root_dir_block = sb.data_block_start;

    // Mark this block as used in the block bitmap
    bbm[0] |= 0x01; // Mark first block as used
    fseek(f, sb.free_block_bitmap * CFS5_BLOCK_SIZE, SEEK_SET);
    fwrite(bbm, 1, CFS5_BLOCK_SIZE, f);

    // 2. Prepare the root directory block with "." and ".." entries
    uint8_t dir_block[CFS5_BLOCK_SIZE];
    memset(dir_block, 0, sizeof(dir_block));

    cfs5_dirent_t* dot_entry = (cfs5_dirent_t*)&dir_block[0];
    dot_entry->inode = 1; // root inode
    strncpy(dot_entry->name, ".", CFS5_MAX_NAME-1);
    dot_entry->name[CFS5_MAX_NAME-1] = '\0';

    cfs5_dirent_t* dotdot_entry = (cfs5_dirent_t*)&dir_block[sizeof(cfs5_dirent_t)];
    dotdot_entry->inode = 1; // root inode (points to itself for root)
    strncpy(dotdot_entry->name, "..", CFS5_MAX_NAME-1);
    dotdot_entry->name[CFS5_MAX_NAME-1] = '\0';

    // 3. Write the root directory block
    fseek(f, root_dir_block * CFS5_BLOCK_SIZE, SEEK_SET);
    fwrite(dir_block, 1, CFS5_BLOCK_SIZE, f);

    // 4. Initialize the root inode (inode 1)
    cfs5_inode_t root_inode;
    memset(&root_inode, 0, sizeof(root_inode));

    root_inode.type = CFS5_INODE_DIR;
    root_inode.mode = 0755; // rwxr-xr-x
    root_inode.uid = 0;     // root
    root_inode.gid = 0;     // root
    root_inode.size = 2 * sizeof(cfs5_dirent_t); // . and .. entries
    root_inode.extent_count = 1;
    root_inode.extents[0].start_block = root_dir_block;
    root_inode.extents[0].block_count = 1;

    // Debug: print the root inode values
    printf("Root inode values:\n");
    printf("  type: %d (0x%x)\n", root_inode.type, root_inode.type);
    printf("  mode: %o (0x%x)\n", root_inode.mode, root_inode.mode);
    printf("  uid: %d\n", root_inode.uid);
    printf("  gid: %d\n", root_inode.gid);
    printf("  size: %d (0x%x)\n", root_inode.size, root_inode.size);
    printf("  extent_count: %d\n", root_inode.extent_count);
    printf("  extent[0].start_block: %d (0x%x)\n", root_inode.extents[0].start_block, root_inode.extents[0].start_block);
    printf("  extent[0].block_count: %d (0x%x)\n", root_inode.extents[0].block_count, root_inode.extents[0].block_count);

    // 5. Write the root inode to the inode table
    // Inode table starts at sb.inode_table_start
    // Each inode is sizeof(cfs5_inode_t) bytes
    // Inode 1 is at offset: sizeof(cfs5_inode_t) * (1) from start of inode table
    uint32_t inode_offset = sb.inode_table_start * CFS5_BLOCK_SIZE + sizeof(cfs5_inode_t);
    printf("Writing root inode at offset: 0x%x (%u)\n", inode_offset, inode_offset);
    fseek(f, inode_offset, SEEK_SET);
    if (fwrite(&root_inode, 1, sizeof(cfs5_inode_t), f) != sizeof(cfs5_inode_t)) {
        perror("fwrite root inode");
        fclose(f);
        return 1;
    }

    // Optional: read back the inode to verify
    cfs5_inode_t check_inode;
    fseek(f, inode_offset, SEEK_SET);
    if (fread(&check_inode, 1, sizeof(cfs5_inode_t), f) != sizeof(cfs5_inode_t)) {
        perror("fread root inode");
        fclose(f);
        return 1;
    }
    printf("Read back root inode values:\n");
    printf("  type: %d (0x%x)\n", check_inode.type, check_inode.type);
    printf("  mode: %o (0x%x)\n", check_inode.mode, check_inode.mode);
    printf("  uid: %d\n", check_inode.uid);
    printf("  gid: %d\n", check_inode.gid);
    printf("  size: %d (0x%x)\n", check_inode.size, check_inode.size);
    printf("  extent_count: %d\n", check_inode.extent_count);
    printf("  extent[0].start_block: %d (0x%x)\n", check_inode.extents[0].start_block, check_inode.extents[0].start_block);
    printf("  extent[0].block_count: %d (0x%x)\n", check_inode.extents[0].block_count, check_inode.extents[0].block_count);

    // Zero out the rest of the inode table (we already zeroed the whole disk,
    // but let's be explicit about the inode table being empty except for root)
    for (uint32_t i = 2; i < sb.inode_count; i++) {
        cfs5_inode_t empty_inode;
        memset(&empty_inode, 0, sizeof(empty_inode));
        fseek(f, sb.inode_table_start * CFS5_BLOCK_SIZE + i * sizeof(cfs5_inode_t), SEEK_SET);
        fwrite(&empty_inode, 1, sizeof(empty_inode), f);
    }

    fclose(f);
    printf("Filesystem created successfully\n");
    return 0;
}