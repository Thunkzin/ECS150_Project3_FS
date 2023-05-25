#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

// Superblock data structure that contains information about filesystem
struct superblock{				// 4096 bytes
	char signature[8];			// Signature
	uint16_t total_disk_blocks;		// Total amount of blocks on virtual disk
	uint16_t root_dir_index;		// Root directory block index
	uint16_t dataBlock_startIndex;	// Data block start index
	uint16_t total_dataBlocks; 		// Amount of data blocks
	uint8_t sizeOf_fat; 			// Number of blocks for FAT
	uint8_t sup_padding[4079];		// Unused or Padding
}__attribute__((packed));			


// FAT data structure with each entry of 2 bytes
uint16_t *fat_entries;				// a pointer to FAT entries


// Root Directory which contains information about the file
struct root_dir{				// 32 bytes
	char file_name[16];			// Filename (including NULL char)
	uint32_t file_size;			// Size of the file
	uint16_t firstDataBlock_index;		// Index of the first data block
	uint8_t root_padding[10];		// Unused or Padding
}__attribute__((packed));

const char virtualDisk = 'ECS150FS';

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

