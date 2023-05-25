#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define SUPERBLOCK_INDEX 0
#define FAT_BLOCK_INDEX 1
#define FAT_EOC OxFFFF


// Superblock data structure which contains information about filesystem
struct superblock{				// 4096 bytes
	char signature[8];			// Signature
	uint16_t total_disk_blocks;		// Total amount of blocks on virtual disk
	uint16_t root_dir_index;		// Root directory block index
	uint16_t dataBlock_startIndex;		// Data block start index
	uint16_t dataBlocks_amount;		// Amount of data blocks
	uint8_t numOf_fatBlocks; 		// Number of blocks for FAT
	uint8_t sblock_padding[4079];		// Unused or Padding
}__attribute__((packed));			


// FAT data structure with each entry of 2 bytes
uint16_t *fat;					// a pointer to FAT 


// Root Directory which contains information about the file
struct root_dir{				// 32 bytes
	char file_name[FS_FILENAME_LEN];	// Filename (including NULL char)
	uint32_t file_size;			// Size of the file
	uint16_t firstDataBlock_index;		// Index of the first data block
	uint8_t root_padding[10];		// Unused or Padding
}__attribute__((packed));


// Global variables
const char virtualDisk[8] = "ECS150FS";		// Declare a constant char array
struct superblock sblock;			// Create a superblock struct instance
struct root_dir fileInfo[FS_FILE_MAX_COUNT];	// Create a root directory instance


int fs_mount(const char *diskname)
{
	// Open the virtual disk file
    	if (block_disk_open(diskname) == -1){	// checking the conditions
		fprintf(stderr, "Cannot open the disk.\n" );
		return -1;
	}
	// Load meta-data from the disk into memory
	// Read the superblock from the first block of the disk
	if(block_read(SUPERBLOCK_INDEX, &sblock) == -1){
		fprintf(stderr, "Failed to read Superblock.\n" );
		return -1;
	}

	// Error handling: checking signature of the file system
	if(strncmp(sblock.signature, virtualDisk, 8) != 0){
		fprintf(stderr, "Signature specification does not match.\n" );
		return -1;
	}

	// Error handling: checking the total block count
	if(sblock.total_disk_blocks != block_disk_count()){
		fprintf(stderr, "block count does not match.\n" );
		return -1;
	}
	
	// Calculate the total entries in FAT blocks: 
	// total number of FAT blocks multiply by the size of entries per block
	size_t totalEntries = sblock.numOf_fatBlocks * BLOCK_SIZE/2;
	// Allocating memory for the FAT data structure
	fat = malloc(sizeof(totalEntries));

	// Read the FAT from second block of thedisk and store it in the buffer
	uint8_t i;
	for(i = 0; i < sblock.numOf_fatBlocks; i++){
		if(block_read(FAT_BLOCK_INDEX + i, fat + i * BLOCK_SIZE/2) == -1);
			fprintf(stderr, "Failed to read FAT block.\n");
			return -1;
	}

	/*
	for (size_t i = 0; i < total_entries; i++){
		if (fat[i] == 0){
			printf("Entry %zu is available.\n", i);
		} else if (fat[i] == FAT_EOC) {
        		printf("Entry %zu is the end of a file.\n", i);
    		} else {
			printf("Entry %zu points to block %u.\n", i, fat[i]);
		}
	}
	*/

	// Read the root directory (block index = FAT'end index + 1)
	if(block_read(sblock.root_dir_index, &fileInfo) == -1){
		fprintf(stderr, "Failed to read root directory.\n");
		return -1;
	}
	
	return 0;
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

