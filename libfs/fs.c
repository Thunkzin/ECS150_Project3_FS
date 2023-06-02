#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

// Constants
#define SUPERBLOCK_INDEX 0
#define FAT_BLOCK_INDEX 1
#define FAT_EOC 0xFFFF
#define FAT_FREE 0
#define min(a, b) ((a) < (b) ? (a) : (b))

// Data structures
struct superblock {
    char signature[8];
    uint16_t total_disk_blocks;
    uint16_t rootDir_blockIndex;
    uint16_t dataBlock_startIndex;
    uint16_t numOf_dataBlocks;
    uint8_t numOf_fatBlocks;
    uint8_t unused[4079];
} __attribute__((packed));

struct fatEntry {
    uint16_t content;
} __attribute__((packed));

struct rootDirEntry {
    char file_name[FS_FILENAME_LEN];
    uint32_t file_size;
    uint16_t firstDataBlock_index;
    uint8_t unused[10];
} __attribute__((packed));

struct fileDescriptor {
    size_t fdOffset;
    int fdIndex;
    int rIndex;
} __attribute__((packed));

// Global variables
struct superblock sblock;
struct fatEntry *fat;
struct rootDirEntry rdir[FS_FILE_MAX_COUNT];
struct fileDescriptor fds[FS_OPEN_MAX_COUNT];
const char myVirtualDisk[8] = "ECS150FS";
int isMounted = 0;

// Helper functions
int find_empty_rIndex(struct rootDirEntry *rDir) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (rDir[i].file_name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

int count_open_fds(void) {
    int open_fds = 0;
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (fds[i].fdIndex != -1) {
            open_fds++;
        }
    }
    return open_fds;
}

int get_data_block_index(int fd) {
    int rootIndex = fds[fd].rIndex;
    int dataIndex = rdir[rootIndex].firstDataBlock_index;
    int realIndex = sblock.dataBlock_startIndex + dataIndex;
    return realIndex;
}

int allocate_new_data_block(void) {
    for (int i = 0; i < sblock.numOf_dataBlocks; i++) {
        if (fat[i].content == FAT_FREE) {
            return i;
        }
    }
    return -1;
}

int fs_mount(const char *diskname) {
    if (block_disk_count() != -1) {
        fprintf(stderr, "A disk is already mounted.\n");
        return -1;
    }

    if (block_disk_open(diskname) == -1) {
        fprintf(stderr, "Cannot open the disk.\n");
        return -1;
    }

    block_read(SUPERBLOCK_INDEX, &sblock);

    if (strncmp(sblock.signature, myVirtualDisk, 8) != 0) {
        fprintf(stderr, "Signature specification does not match.\n");
        return -1;
    }

    if (sblock.total_disk_blocks != block_disk_count()) {
        fprintf(stderr, "Block count does not match.\n");
        return -1;
    }

    fat = malloc(sblock.numOf_fatBlocks * BLOCK_SIZE);
    if (fat == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return -1;
    }

    for (int i = 0; i < sblock.numOf_fatBlocks; i++) {
        block_read(i + FAT_BLOCK_INDEX, fat + (i * BLOCK_SIZE));
    }

    block_read(sblock.rootDir_blockIndex, rdir);

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        fds[i].fdIndex = -1;
        fds[i].rIndex = -1;
    }

    isMounted = 1;

    return 0;
}

int fs_umount(void) {
    if (isMounted == 0) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    for (int i = 0; i < sblock.numOf_fatBlocks; i++) {
        block_write(i + FAT_BLOCK_INDEX, fat + (i * BLOCK_SIZE));
    }

    block_write(sblock.rootDir_blockIndex, rdir);

    free(fat);
    fat = NULL;

    if (block_disk_close() == -1) {
        fprintf(stderr, "Disk closing failed.\n");
        return -1;
    }

    isMounted = 0;

    return 0;
}

int fs_create(const char *filename) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    if (strlen(filename) >= FS_FILENAME_LEN) {
        fprintf(stderr, "Filename is too long.\n");
        return -1;
    }

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strncmp(rdir[i].file_name, filename, FS_FILENAME_LEN) == 0) {
            fprintf(stderr, "File already exists.\n");
            return -1;
        }
    }

    int rIndex = find_empty_rIndex(rdir);
    if (rIndex == -1) {
        fprintf(stderr, "Cannot create file. Maximum number of files reached.\n");
        return -1;
    }

    strncpy(rdir[rIndex].file_name, filename, FS_FILENAME_LEN);
    rdir[rIndex].file_size = 0;
    rdir[rIndex].firstDataBlock_index = FAT_EOC;

    return 0;
}

int fs_delete(const char *filename) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    int found = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strncmp(rdir[i].file_name, filename, FS_FILENAME_LEN) == 0) {
            rdir[i].file_name[0] = '\0';
            rdir[i].file_size = 0;
            rdir[i].firstDataBlock_index = FAT_EOC;
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "File not found.\n");
        return -1;
    }

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (fds[i].rIndex != -1 && strncmp(rdir[fds[i].rIndex].file_name, filename, FS_FILENAME_LEN) == 0) {
            fs_close(i);
        }
    }

    return 0;
}

int fs_ls(void) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (rdir[i].file_name[0] != '\0') {
            printf("Filename: %s, Size: %d\n", rdir[i].file_name, rdir[i].file_size);
        }
    }

    return 0;
}

int fs_open(const char *filename) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    int rIndex = -1;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strncmp(rdir[i].file_name, filename, FS_FILENAME_LEN) == 0) {
            rIndex = i;
            break;
        }
    }

    if (rIndex == -1) {
        fprintf(stderr, "File not found.\n");
        return -1;
    }

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (fds[i].fdIndex == -1) {
            fds[i].fdOffset = 0;
            fds[i].fdIndex = i;
            fds[i].rIndex = rIndex;
            return i;
        }
    }

    fprintf(stderr, "Too many open files.\n");
    return -1;
}

int fs_close(int fd) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    if (fds[fd].fdIndex == -1) {
        fprintf(stderr, "File descriptor is not open.\n");
        return -1;
    }

    fds[fd].fdIndex = -1;
    fds[fd].rIndex = -1;
    return 0;
}

int fs_stat(int fd) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    if (fds[fd].fdIndex == -1) {
        fprintf(stderr, "File descriptor is not open.\n");
        return -1;
    }

    int rIndex = fds[fd].rIndex;
    printf("Filename: %s, Size: %d\n", rdir[rIndex].file_name, rdir[rIndex].file_size);
    return 0;
}

int fs_lseek(int fd, size_t offset) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    if (fds[fd].fdIndex == -1) {
        fprintf(stderr, "File descriptor is not open.\n");
        return -1;
    }

    int rIndex = fds[fd].rIndex;
    if (offset > rdir[rIndex].file_size) {
        fprintf(stderr, "Offset exceeds file size.\n");
        return -1;
    }

    fds[fd].fdOffset = offset;
    return 0;
}

int fs_write(int fd, void *buf, size_t count) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    if (fds[fd].fdIndex == -1) {
        fprintf(stderr, "File descriptor is not open.\n");
        return -1;
    }

    int rIndex = fds[fd].rIndex;
    size_t offset = fds[fd].fdOffset;

    // Get the data block index for the current offset
    int dataBlockIndex = get_data_block_index(fd);

    // Find the last data block in the file
    int lastDataBlockIndex = dataBlockIndex;
    while (fat[lastDataBlockIndex].content != FAT_EOC) {
        lastDataBlockIndex = fat[lastDataBlockIndex].content;
    }

    int bytes_written = 0;

    while (count > 0) {
        // Calculate the remaining space in the current data block
        int block_offset = offset % BLOCK_SIZE;
        int space_in_block = BLOCK_SIZE - block_offset;

        // Calculate the amount of data to write in the current iteration
        int write_size = min(count, space_in_block);

        // If there is no space left in the current data block, allocate a new one
        if (block_offset == 0 && space_in_block == BLOCK_SIZE) {
            int new_data_block = allocate_new_data_block();
            if (new_data_block == -1) {
                fprintf(stderr, "No free data blocks available.\n");
                return -1;
            }

            // Update the FAT to link the new data block to the file
            if (lastDataBlockIndex == dataBlockIndex) {
                rdir[rIndex].firstDataBlock_index = new_data_block;
            } else {
                fat[lastDataBlockIndex].content = new_data_block;
            }

            // Update the lastDataBlockIndex to the new data block
            lastDataBlockIndex = new_data_block;
            fat[lastDataBlockIndex].content = FAT_EOC;
        }

        // Calculate the real index of the data block to write to
        int realIndex = sblock.dataBlock_startIndex + lastDataBlockIndex;

        // Read the current data block from the disk
        char data_block[BLOCK_SIZE];
        block_read(realIndex, data_block);

        // Copy the data from the buffer to the current data block
        memcpy(data_block + block_offset, buf, write_size);

        // Write the updated data block back to the disk
        block_write(realIndex, data_block);

        // Update the offset, count, and bytes_written
        offset += write_size;
        count -= write_size;
        bytes_written += write_size;

        // Move to the next data block if necessary
        buf += write_size;

        // If the current data block was filled completely, update the lastDataBlockIndex
        if (block_offset + write_size == BLOCK_SIZE) {
            lastDataBlockIndex = fat[lastDataBlockIndex].content;
        }
    }

    // Update the file size
    if (offset > rdir[rIndex].file_size) {
        rdir[rIndex].file_size = offset;
    }

    // Update the file offset
    fds[fd].fdOffset = offset;

    return bytes_written;
}

int fs_read(int fd, void *buf, size_t count) {
    if (!isMounted) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;
    }

    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    if (fds[fd].fdIndex == -1) {
        fprintf(stderr, "File descriptor is not open.\n");
        return -1;
    }

    int rIndex = fds[fd].rIndex;
    size_t offset = fds[fd].fdOffset;

    int dataBlockIndex = get_data_block_index(fd);

    int bytes_read = 0;

    while (count > 0 && offset < rdir[rIndex].file_size) {
        // Calculate the remaining data in the current data block
        int block_offset = offset % BLOCK_SIZE;
        int data_in_block = BLOCK_SIZE - block_offset;

        // Calculate the amount of data to read in the current iteration
        int read_size = min(count, data_in_block);

        // Calculate the real index of the data block to read from
        int realIndex = sblock.dataBlock_startIndex + dataBlockIndex;

        // Read the data block from the disk
        char data_block[BLOCK_SIZE];
        block_read(realIndex, data_block);

        // Copy the data from the current data block to the buffer
        memcpy(buf, data_block + block_offset, read_size);

        // Update the offset, count, bytes_read, and buffer pointer
        offset += read_size;
        count -= read_size;
        bytes_read += read_size;
        buf += read_size;

        // Move to the next data block if necessary
        if (block_offset + read_size == BLOCK_SIZE) {
            dataBlockIndex = fat[dataBlockIndex].content;
        }
    }

    // Update the file offset
    fds[fd].fdOffset = offset;

    return bytes_read;
}
