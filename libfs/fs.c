#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define SUPERBLOCK_INDEX 0
#define FAT_BLOCK_INDEX 1
#define FAT_EOC 0xFFFF
#define FAT_FREE 0

/* TODO: Phase 1 */

// Superblock data structure which contains information about filesystem
struct superblock{					
	char signature[8];			// Signature
	uint16_t total_disk_blocks;		// Total amount of blocks on virtual disk
	uint16_t rootDir_blockIndex;		// Root directory block index
	uint16_t dataBlock_startIndex;		// Data block start index
	uint16_t numOf_dataBlocks;		// Amount of data blocks
	uint8_t numOf_fatBlocks; 		// Number of blocks for FAT
	uint8_t unused[4079];			// Unused or Padding
}__attribute__((packed));		

// FAT entry data structure
struct fatEntry {		// 2 bytes per entry
    uint16_t content;		// fat entry stores the index of the next data block
}__attribute__((packed));

// A single root directory entry which contains information about the file
struct rootDirEntry{				// 32 bytes per entry
	char file_name[FS_FILENAME_LEN];	// Filename (including NULL char) 16bytes 
	uint32_t file_size;			// Size of the file
	uint16_t firstDataBlock_index;		// Index of the first data block
	uint8_t unused[10];			// Unused or Padding
}__attribute__((packed));

// File descriptor data structure
struct fileDescriptor {	
    size_t fdOffset;
	int fdIndex;	// -1 for closed or unused fd
    int rIndex;		// index of the file in root directory
}__attribute__((packed));
    
// Global instances and variables
struct superblock sblock;
struct fatEntry *fat;
struct rootDirEntry rdir[FS_FILE_MAX_COUNT];	// Total of 128 entries 
struct fileDescriptor fds[FS_OPEN_MAX_COUNT];	// Total of 32 open file descriptors
const char myVirtualDisk[8] = "ECS150FS";	// Declare a constant char array
int isMounted = 0;				// Flag to track if filesystem is currently mounted


// Helper function prototypes
int count_free_fat_entries(void);			// Function to count free FAT entries
int count_free_root_dir_entries(void);			// Function to count free root directory entries
int find_empty_rIndex(struct rootDirEntry *rDir);	// Function to find an empty entry index in root directory
int count_open_fds(void);				// Function to keep track of opened file descriptors
int get_data_block_index();



/* Helper function definitions */
// Function to count free fat entries
int count_free_fat_entries(void) {			// Use in fs_info() 
    int free_fat_count = 0;				// Initialize a variable to store fat free count
    for(int i = 0; i < sblock.numOf_dataBlocks; i++) {	// since there are as many entries as data blocks in the disk
        if(fat[i].content == FAT_FREE) {		// Assuming 0: corresponds to fat free entry
            free_fat_count++;				// Increment the count
        }
    }
    return free_fat_count;
} // end of count_free_fat_entries function

// Function to count free root directory entries
int count_free_root_dir_entries(void) {			// Use in fs_info() 
    int free_root_dir_count = 0;			// Initialize a variable to store free root directory count
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {	// Iterate over 128 entries of the root directory 
        if(rdir[i].file_name[0] == '\0') {		// Assumimg empty file as free entry
            free_root_dir_count++;			// Increment the count
        }
    }
    return free_root_dir_count;
} // end of count_free_root_dir_entries

// Function to find the position of an empty entry to create a file in the root directory.
int find_empty_rIndex(struct rootDirEntry *rDir) {	// Use in fs_create()
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {	
        if (rdir[i].file_name[0] == '\0') {
            return i;					// Returns the index of the empty entry
        }
    }
    return -1;						// or -1 if no empty entry was found.
}

// Function to count open file descriptors
int count_open_fds(void){				// Use in fs_open()
	int open_fds = 0;
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++){
		if(fds[i].fdIndex != -1){		// -1 means fd is closed or unused.
			open_fds++;			// Increment the count
		
	}
	return open_fds;				// return the number of currently opened fds
}


/* TODO: Phase 1 */
int fs_mount(const char *diskname)
{
	// Check if a disk is already open
    if(block_disk_count() != -1){
        fprintf(stderr, "A disk is already mounted.\n");
        return -1;
    }
	
	// Open the virtual disk file
    if(block_disk_open(diskname) == -1){		// checking the condition
		fprintf(stderr, "Cannot open the disk.\n" );
        return -1;
    }
    
    // Load meta-data from the disk into memory

	// Read the superblock from the first block of the disk
	if(block_read(SUPERBLOCK_INDEX, &sblock) == -1){
		fprintf(stderr, "Failed to read Superblock.\n" );
		return -1;
	}

	// Error handling: check the signature of the file system
	if(strncmp(sblock.signature, myVirtualDisk, 8) != 0){
		fprintf(stderr, "Signature specification does not match.\n" );
		return -1;
	}

	// Error handling: check if the data block counts is correct
	if(sblock.total_disk_blocks != block_disk_count()){
		fprintf(stderr, "block count does not match.\n" );
		return -1;
	}

	// Allocate memory for the FAT data structure 
	/* 
	Calculation:
	Total FAT memory to allocate = total number of blocks occupied by the FAT in the disk * size of one FAT block 
	*/
	fat = malloc(sblock.numOf_fatBlocks * BLOCK_SIZE / sizeof(struct fatEntry));
	if(fat == NULL){		// Check if FAT memory allocation is successful
		return -1;
	}

	// Read each block of the FAT from the disk and store it in the allocated memory.
	for(uint8_t i = 0; i < sblock.numOf_fatBlocks; i++){
		if(block_read(FAT_BLOCK_INDEX + i,fat + i * BLOCK_SIZE / sizeof(struct fatEntry)) == -1){
			fprintf(stderr, "Failed to read FAT block.\n");
			free(fat);
			return -1;
		}
	}

	// Read the root directory from disk 
	if (block_read(sblock.rootDir_blockIndex, &rdir) == -1) {
		fprintf(stderr, "Failed to read root directory.\n");
		return -1;
	}

	// Initialize the file descriptors
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++){
		fds[i].fdIndex = -1;	// Mark all file descriptors as unused
		fds[i].rIndex = -1;	// Initialize rIndex to invalid value
		fds[i].fdOffset = 0;	// Initialize offset to zero
	}

	isMounted = 1;	// Mark as mounted

	return 0; // success
}

/* TODO: Phase 1 */
int fs_umount(void)
{
/**
 * fs_umount - Unmount file system
 *
 * Unmount the currently mounted file system and close the underlying virtual
 * disk file.
 *
 * Return: -1 if no FS is currently mounted, or if the virtual disk cannot be
 * closed, or if there are still open file descriptors. 0 otherwise.
 */

	// Check if no FS is currently mounted  // ??? block_disk_count? or block_disk_close?
    if(block_disk_close() == -1) {
        fprintf(stderr, "No file system is currently mounted.\n");
        return -1;
    }

	// Check if there are still open file descriptors
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (fds[i].fdIndex != -1) {
            fprintf(stderr, "There are still open file descriptors.\n");
            return -1;
        }
    }

	// Write root directory information back to disk
	if(block_write(sblock.rootDir_blockIndex, &rdir) == -1){
		fprintf(stderr, "Failed to write root directory to disk.\n");
		return -1;
	}

    // Write FAT information back to disk
    for (uint8_t i = 0; i < sblock.numOf_fatBlocks; i++) {
        if (block_write(FAT_BLOCK_INDEX + i, fat + i * BLOCK_SIZE/2) == -1) {
            fprintf(stderr, "Failed to write FAT to disk.\n");
            return -1;
        }
    }

    // Free FAT from memory
	if(fat != NULL){
		 free(fat);
		 fat = NULL;
	}

	// Close the underlying virtual disk
	if(block_disk_close() == -1){
		fprintf(stderr, "Failed to close the underlying virtual disk.\n");
		return -1;
	}

	isMounted = 0;	// Mark as unmounted

	return 0; // unmounted successful
}

/* TODO: Phase 1 */
int fs_info(void)
{
/* 
 * Reference program output: 
 * tkzin@COE-CS-pc1:~/p3/apps$ fs_ref.x info disk.fs
 * FS Info:
 * total_blk_count=4100
 * fat_blk_count=2
 * rdir_blk=3
 * data_blk=4
 * data_blk_count=4096
 * fat_free_ratio=4095/4096
 * rdir_free_ratio=128/128
 */
	// Check if no FS is currently mounted
    if(isMounted == 0){
        fprintf(stderr, "No file system is currently mounted.\n");
        return -1;
    }

	// Calculate free FAT entries
    int free_fat_count = count_free_fat_entries();

    // Calculate free root directory entries
    int free_root_dir_count = count_free_root_dir_entries();

	// Print out the FS info: 
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", sblock.total_disk_blocks);
    printf("fat_blk_count=%d\n", sblock.numOf_fatBlocks);
    printf("rdir_blk=%d\n", sblock.rootDir_blockIndex);
    printf("data_blk=%d\n", sblock.dataBlock_startIndex);
    printf("data_blk_count=%d\n", sblock.numOf_dataBlocks);
    printf("fat_free_ratio=%d/%d\n", free_fat_count, sblock.numOf_dataBlocks);
    printf("rdir_free_ratio=%d/%d\n", free_root_dir_count, FS_FILE_MAX_COUNT);

	return 0; // success
}

/* TODO: Phase 2 */
int fs_create(const char *filename)
{
/**
 * fs_create - Create a new file
 * @filename: File name
 *
 * Create a new and empty file named @filename in the root directory of the
 * mounted file system. String @filename must be NULL-terminated and its total
 * length cannot exceed %FS_FILENAME_LEN characters (including the NULL
 * character).
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if a
 * file named @filename already exists, or if string @filename is too long, or
 * if the root directory already contains %FS_FILE_MAX_COUNT files. 0 otherwise.
 */
	// Check if no FS is currently mounted
    if(isMounted == 0){
        fprintf(stderr, "No FS currently mounted.\n");
        return -1;
    }

	// Check if the filename valid or too long
	if(filename == NULL || strlen(filename) > FS_FILENAME_LEN){	
		return -1;
	}

	// Check if a file with the same name already exists
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if(strcmp(rdir[i].file_name, filename) == 0) {
            fprintf(stderr, "File with the same name already exists.\n");
            return -1;
        }
	}

	// Check if the root directory exceeded FS_FILE_MAX_COUNT files
	int remptyIndex = find_empty_rIndex(rdir);
	if(remptyIndex == -1){	// -1 means no empty entry, root directory is full.
		return -1;
	}

	// Now, create a new empty file with a given parameter @filename 
	// at the free index we just found in the root directory
	strncpy(rdir[remptyIndex].file_name, filename, FS_FILENAME_LEN);	// get the filename
	rdir[remptyIndex].file_size = 0; 					// set the file size to zero
	rdir[remptyIndex].firstDataBlock_index = FAT_EOC;			// set first data block to end of chain


	// Update the root directory information back in the disk
	if(block_write(sblock.rootDir_blockIndex, &rdir) == -1){
		return -1;
	}

	return 0; // fs_create success

}

int fs_delete(const char *filename)
{
/* TODO: Phase 2 */
/**
 * fs_delete - Delete a file
 * @filename: File name
 *
 * Delete the file named @filename from the root directory of the mounted file
 * system.
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if
 * Return: -1 if @filename is invalid, if there is no file named @filename to
 * delete, or if file @filename is currently open. 0 otherwise.
 */

	// Check if no FS is currently mounted
    if(isMounted == 0){
        fprintf(stderr, "No FS currently mounted.\n");
        return -1;
    }

	// Check if the filename is valid or too long
	if(filename == NULL || strlen(filename) > FS_FILENAME_LEN){	
		fprintf(stderr, "Invalid filename.\n");
		return -1;
	}

	// Check if the given parameter @filename exists in root directory to delete?
	int found = -1;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(strcmp(rdir[i].file_name, filename) == 0){	// Iterate over 128 files and compare their filenames
			found = i;				// if found the index with the same name, store it in 'found' var
			break;
		}
	}
	// if the filename is not found, return -1.
	if(found == -1){	
		fprintf(stderr, "Filename does not exist.\n");
		return -1;	
	}

	// Check if the file is already open
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++){
		if(fds[i].rIndex == found){
			fprintf(stderr, "File is currently open.\n");
			return -1;
		}
	}

	// Once the file is found, empty the file's entry in the root directory
	memset(&rdir[found], 0, sizeof(struct rootDirEntry));


	// Delete the file's data blocks used by the file
	// First we need to find the first index of the data block stored in the FAT
	// and move to the next block until the fat entry is not equal to FAT_EOC.
	int currentFatEntry = rdir[found].firstDataBlock_index;
	while(currentFatEntry != FAT_EOC){
		int nextFatEntry = fat[currentFatEntry].content;
		fat[currentFatEntry].content = FAT_FREE;
		currentFatEntry = nextFatEntry;
	}

	return 0;
}

int fs_ls(void)
{
/* TODO: Phase 2 */
/**
 * fs_ls - List files on file system
 *
 * List information about the files located in the root directory.
 *
 * Return: -1 if no FS is currently mounted. 0 otherwise.
 */

	// Check if no FS is currently mounted
    if(isMounted == 0){
        fprintf(stderr, "No FS currently mounted.\n");
        return -1;
    }

	// Display the lists of all the files in the file system.
	printf("FS Ls:\n");

	// Iterate over the entries in the root directory
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
		// If the filename is not empty, print out
		if(rdir[i].file_name[0] != '\0'){
			printf("file: %s, size: %d, data_blk: %d\n", 
			rdir[i].file_name, rdir[i].file_size, rdir[i].firstDataBlock_index);

		}
	}
	return 0;
}

/* TODO: Phase 3 */
int fs_open(const char *filename)
{
/**
 * fs_open - Open a file
 * @filename: File name
 *
 * Open file named @filename for reading and writing, and return the
 * corresponding file descriptor. The file descriptor is a non-negative integer
 * that is used subsequently to access the contents of the file. The file offset
 * of the file descriptor is set to 0 initially (beginning of the file). If the
 * same file is opened multiple files, fs_open() must return distinct file
 * descriptors. A maximum of %FS_OPEN_MAX_COUNT files can be open
 * simultaneously.
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if
 * there is no file named @filename to open, or if there are already
 * %FS_OPEN_MAX_COUNT files currently open. Otherwise, return the file
 * descriptor.
 */	

	// Check if no FS is currently mounted
    if(isMounted == 0){
        fprintf(stderr, "No FS currently mounted.\n");
        return -1;
    }

	// Check if the filename is valid or too long
	if(filename == NULL || strlen(filename) > FS_FILENAME_LEN){
		fprintf(stderr, "Invalid filename.\n");
		return -1;
	}

	// Check if the given input @filename exists in root directory 
	int found = -1;		
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(strcmp(rdir[i].file_name, filename) == 0){	// Iterate through 128 files and compare their filenames
			found = i;				// update root directory index 
			break;
		}
	}
	// If the filename is not found, return -1.
	if(found == -1){	
		fprintf(stderr, "Filename does not exist.\n");
		return -1;	
	}

	// Check if there are already FS_OPEN_MAX_COUNT files currently open
	if(count_open_fds() >= FS_OPEN_MAX_COUNT){
		fprintf(stderr, "Maximum open file limit reached.\n");
		return -1;
	}

	// Find the first availabe location in file descriptor data structure
	int loc = -1;
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++){
		if(fds[i].fdIndex == -1){	// if unused index is found,
			loc = i;		// return the location.
			break;
		}
	}

	// Initialize the file descriptor's values at the available location
	fds[loc].fdOffset = 0;
	fds[loc].fdIndex = loc;		
	fds[loc].rIndex = found;	// assign it to the file Index that matches with the input filename in rd.


	return fds[loc].fdIndex;	// return open fd 
}

/* TODO: Phase 3 */
int fs_close(int fd)
{
/**
 * fs_close - Close a file
 * @fd: File descriptor
 *
 * Close file descriptor @fd.
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open). 0 otherwise.
 */

	// Check if no FS is currently mounted
    if(isMounted == 0){
        fprintf(stderr, "No FS currently mounted.\n");
        return -1;
    }

	// Check if @fd is valid (out of bounds or not currently open)
	// if @fd is non-negative integer, invalid.
	// if @fd exceeds maximum open count, invalid.
	// if @fd is -1, it means unused or closed.
	if(fd < 0 || fd >= FS_OPEN_MAX_COUNT || fds[fd].fdIndex == -1){
		return -1;
	}

	// Close the file descriptor by setting to -1 and offset to 0
	fds[fd].fdIndex = -1;
	fds[fd].rIndex = -1;
	fds[fd].fdOffset = 0;
			
	return 0;	// success 
}

/* TODO: Phase 3 */
int fs_stat(int fd)
{
/**
 * fs_stat - Get file status
 * @fd: File descriptor
 *
 * Get the current size of the file pointed by file descriptor @fd.
 *
 * Return: -1 if no FS is currently mounted, of if file descriptor @fd is
 * invalid (out of bounds or not currently open). Otherwise return the current
 * size of file.
 */

	// Check if no FS is currently mounted
    if(isMounted == 0){
        fprintf(stderr, "No FS currently mounted.\n");
        return -1;
    }

	// Check if @fd is valid (out of bounds or not currently open)
	// if @fd is non-negative integer, invalid.
	// if @fd exceeds maximum open count, invalid.
	// if @fd is -1, it means unused or closed.
	if(fd < 0 || fd >= FS_OPEN_MAX_COUNT || fds[fd].fdIndex == -1){
		return -1;
	}

	// Get the index in the root directory to access the file size
	int rootIndex = fds[fd].rIndex;

	// Return the file size from the corresponding root directory entry
	return rdir[rootIndex].file_size;
}

/* TODO: Phase 3 */
int fs_lseek(int fd, size_t offset)
{
/**
 * fs_lseek - Set file offset
 * @fd: File descriptor
 * @offset: File offset
 *
 * Set the file offset (used for read and write operations) associated with file
 * descriptor @fd to the argument @offset. To append to a file, one can call
 * fs_lseek(fd, fs_stat(fd));
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (i.e., out of bounds, or not currently open), or if @offset is larger
 * than the current file size. 0 otherwise.
 */

	// Check if FS is currently mounted
	if(isMounted == 0){
        fprintf(stderr, "No FS currently mounted.\n");
        return -1;
    }

	// Check if @fd is valid (out of bounds, or not currently open)
	if(fd < 0 || fd >= FS_OPEN_MAX_COUNT || fds[fd].fdIndex == -1){
		return -1;
	}

	// Get the current file size 
	int current_fileSize = fs_stat(fd);
	if(current_fileSize == -1){
		// If fs_stat returns -1, there was an error getting the file size
		return -1;
	}

	// Check if @offset is larger than the current file size
	if(offset > current_fileSize){
		return -1;
	}

	// Update the offset in the file descriptor
	fds[fd].fdOffset = offset;

    return 0;
}

/* TODO: Phase 4 */
int fs_write(int fd, void *buf, size_t count)
{
/**
 * fs_write - Write to a file
 * @fd: File descriptor
 * @buf: Data buffer to write in the file
 * @count: Number of bytes of data to be written
 *
 * Attempt to write @count bytes of data from buffer pointer by @buf into the
 * file referenced by file descriptor @fd. It is assumed that @buf holds at
 * least @count bytes.
 *
 * When the function attempts to write past the end of the file, the file is
 * automatically extended to hold the additional bytes. If the underlying disk
 * runs out of space while performing a write operation, fs_write() should write
 * as many bytes as possible. The number of written bytes can therefore be
 * smaller than @count (it can even be 0 if there is no more space on disk).
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open), or if @buf is NULL. Otherwise
 * return the number of bytes actually written.
 */

	// Check if FS is currently mounted
	if(isMounted == 0){
		fprintf(stderr, "No FS currently mounted.\n");
		return 0;
	}

	// Check if file descriptor is valid or out of bounds or not currently open
	if(fd < 0 || fd >= FS_OPEN_MAX_COUNT || fds[fd].fdIndex == -1){
		fprintf(stderr, "Invalid file descriptor.\n");
		return -1;
	}

	// Check if the buffer is full
	if(buf == NULL){
		fprintf(stderr, "Buffer is NULL.\n");
		return 0;
	}
	
}

/* TODO: Phase 4 */
int fs_read(int fd, void *buf, size_t count)
{
/**
 * fs_read - Read from a file
 * @fd: File descriptor
 * @buf: Data buffer to be filled with data
 * @count: Number of bytes of data to be read
 *
 * Attempt to read @count bytes of data from the file referenced by file
 * descriptor @fd into buffer pointer by @buf. It is assumed that @buf is large
 * enough to hold at least @count bytes.
 *
 * The number of bytes read can be smaller than @count if there are less than
 * @count bytes until the end of the file (it can even be 0 if the file offset
 * is at the end of the file). The file offset of the file descriptor is
 * implicitly incremented by the number of bytes that were actually read.
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open), or if @buf is NULL. Otherwise
 * return the number of bytes actually read.
 */

	/*
	Step 1: Find the data block index. Implement a helper function calculates 
	the index of the data block corresponding to the file's current offset. 
	This function will need to take into account the size of the blocks and 
	the file's offset.

	Step 2: Writing: First, read the block from disk into the bounce buffer. 
	Then, modify the part of the buffer that starts from the file's offset 
	with the data from the user-supplied buffer. Finally, write the modified 
	block back to disk.
	*/

	// Check if FS is currently mounted
	if(isMounted == 0){
		fprintf(stderr, "No FS currently mounted.\n");
		return 0;
	}

	// Check if file descriptor is valid or out of bounds or not currently open
	if(fd < 0 || fd >= FS_OPEN_MAX_COUNT || fds[fd].fdIndex == -1){
		fprintf(stderr, "Invalid file descriptor.\n");
		return -1;
	}

	// Check if the user buffer is full
	if(buf == NULL){
		fprintf(stderr, "Buffer is NULL.\n");
		return 0;
	}

	

	
	
}

