#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define SIGNATURE "ECS150FS"

#define fs_error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

typedef struct __attribute__((__packed__)) superBlock{
    char signature[8];
    uint16_t totalBlock;
    uint16_t rootIndex;
    uint16_t dataStartIndex;
    uint16_t numDataBlock;
    uint8_t numFATBlock;
    int8_t unused[4079];
}sBlock;

typedef sBlock* sBlock_t;

typedef struct __attribute__((__packed__)) entryOfRootDirectory{
    char filename[16];
    uint32_t size;
    uint16_t startIndex;
    int8_t unused[10];
}fileInfo;

typedef fileInfo* fileInfo_t;

//we guarantee that file buffer's cotent is always
//the same as the cotent in the virtual disk
typedef struct file_buffer{
    void *base;
    int count; //count how many fd is associated with this file
}fb;

typedef fb* fb_t;

//whenever we create a file descriptor
//we will buffer data of that file
typedef struct file_descriptor{
    int fileID;
    fb_t file;
    void *offset;
}fileDes;

typedef fileDes* fileDes_t;

typedef struct virtualDisk{
    sBlock_t superBlock;
    uint16_t *arrFAT;
    fileInfo_t rootDir;
    fileDes_t FDT;
    int freeFd;
    int freeFATEntries;
    int freeRootEntries;
}vDisk;

static vDisk disk = {.superBlock = NULL,
                        .arrFAT = NULL,
                        .rootDir = NULL,
                        .FDT = NULL};

int fs_mount(const char *diskname)
{
	if(block_disk_open(diskname))
	    return -1;

	sBlock_t superBlock = malloc(BLOCK_SIZE);
	if(!superBlock){
	    perror("malloc");
	    return -1;
	}
	block_read(0, superBlock);

	//error-checking super block
	uint16_t correctFATBlock = (2 * superBlock->numDataBlock + BLOCK_SIZE - 1) / BLOCK_SIZE;
	uint16_t correctDataBlock = block_disk_count() - correctFATBlock - 2;
    //change char array to string
	char *tmp = malloc(9);
    memcpy(tmp, superBlock->signature,8);
    tmp[8] = '\0';
	if(strcmp(tmp, SIGNATURE) != 0
	    || superBlock->totalBlock != block_disk_count()
	    || superBlock->numFATBlock != correctFATBlock
	    || superBlock->numDataBlock != correctDataBlock
	    || superBlock->rootIndex != correctFATBlock + 1
	    || superBlock->dataStartIndex != superBlock->rootIndex + 1)
    {
	    free(tmp);
	    free(superBlock);
	    return -1;
    }
	free(tmp);

	//read File Allocation Table
	//arrFAT should have the same size as the total numFATBlock
	uint16_t *arrFAT = malloc(superBlock->numFATBlock * BLOCK_SIZE);
	if(!arrFAT){
	    free(superBlock);
	    perror("malloc");
	    return -1;
	}

    for (int i = 0; i < superBlock->numFATBlock; ++i)
        block_read(i + 1, (char*)arrFAT + i * BLOCK_SIZE);

    //error check for FAT
    if(arrFAT[0] != FAT_EOC){
        free(superBlock);
        free(arrFAT);
        return -1;
    }

    //compute FAT free number
    int freeFATEntries = 0;
    for (int k = 0; k < superBlock->numDataBlock; ++k) {
        if(arrFAT[k] == 0)
            ++freeFATEntries;
    }

    //read root directory
    fileInfo_t rootDir = malloc(BLOCK_SIZE);
    if(!rootDir){
        free(superBlock);
        free(arrFAT);
        perror("malloc");
        return -1;
    }
    block_read(superBlock->rootIndex, rootDir);

    //error check for root directory
    //compute root directory free number
    int freeRootEntries = 0;
    for (int j = 0; j < FS_FILE_MAX_COUNT; ++j) {
        if(rootDir[j].filename[0] == '\0') {
            ++freeRootEntries;
            continue;
        }
        //if rootDir[j] is not an empty entry
        //and its entries have the correct format
        if((rootDir[j].size > 0 && rootDir[j].startIndex != FAT_EOC)
            || (rootDir[j].size == 0 && rootDir[j].startIndex == FAT_EOC))
            continue;

        free(superBlock);
        free(arrFAT);
        free(rootDir);
        return -1;
    }

    //create FDT and initialize them
    fileDes_t FDT = malloc(FS_OPEN_MAX_COUNT * sizeof(fileDes));
    if(!FDT){
        free(rootDir);
        free(superBlock);
        free(arrFAT);
        perror("malloc");
        return -1;
    }
    for (int l = 0; l < FS_OPEN_MAX_COUNT; ++l){
        //we use -1 indicates that entry is free
        FDT[l].fileID = -1;
        FDT[l].file = NULL;
        FDT[l].offset = NULL;
    }

    //initialize global variable disk
    disk.superBlock = superBlock;
    disk.arrFAT = arrFAT;
    disk.rootDir = rootDir;
    disk.FDT = FDT;
    disk.freeFd = FS_OPEN_MAX_COUNT;
    disk.freeFATEntries = freeFATEntries;
    disk.freeRootEntries = freeRootEntries;

    return 0;
}

int fs_umount(void)
{
    //no virtual disk is opened or disk close fail
    if(!disk.superBlock || disk.freeFd < FS_OPEN_MAX_COUNT || block_disk_close())
        return -1;

    //free everything and quit
    //all file has been close, so we can directly free FDT
    free(disk.superBlock);
    free(disk.arrFAT);
    free(disk.rootDir);
    free(disk.FDT);
    disk.superBlock = NULL;
    disk.arrFAT = NULL;
    disk.rootDir = NULL;
    disk.FDT = NULL;
    return 0;
}

int fs_info(void)
{
    if(!disk.superBlock) {
        return -1;
    }

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", disk.superBlock->totalBlock);
	printf("fat_blk_count=%d\n", disk.superBlock->numFATBlock);
	printf("rdir_blk=%d\n", disk.superBlock->rootIndex);
	printf("data_blk=%d\n", disk.superBlock->dataStartIndex);
	printf("data_blk_count=%d\n", disk.superBlock->numDataBlock);
	printf("fat_free_ratio=%d/%d\n", disk.freeFATEntries, disk.superBlock->numDataBlock);
	printf("rdir_free_ratio=%d/%d\n", disk.freeRootEntries, FS_FILE_MAX_COUNT);
    return 0;
}


 /*
  * return -1 if filename is invalid.
  * return 0 if filename is valid.
  *
  * filename is invalid if and only if:
  * 1, filename is NULL
  * 2, filename[0] == '\0'
  * 3, filename is not NULL-terminated
  * 4, filename's length >= FILENAME_MAX
  *     Note that filename's length == FILENAME_MAX is invalid
  *     because FILENAME_MAX already includes '\0'
  *     but '\0' does not count when we calculate filename's length
  */
int check_filename(const char *filename)
{
    if(!filename || filename[0] == '\0')
        return -1;

    //check if it is null-terminated and if it is too long
    for (int i = 0; i < FS_FILENAME_LEN; ++i) {
        if(filename[i] == '\0')
            return 0;
    }
    return -1;
}

/*
 * check if @filename exists in root directory
 *
 * If it exists, return 0.
 * If it does not exist, return -1.
 */
int check_file_exist(const char *filename)
{
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if(strcmp(disk.rootDir[i].filename, filename) == 0)
            return 0;
    }
    return -1;
}

/*
 * get the index of first free entry in root directory
 * guarantee that root directory is not full when we
 * are calling this function.
 * If it is full, we return -1 indicates there is no
 * free entry
 */
int get_first_free_entry()
{
    assert(disk.freeRootEntries > 0);
    int index;
    for (index = 0; index < FS_FILE_MAX_COUNT; ++index) {
        if(disk.rootDir[index].filename[0] == '\0')
            return index;
    }
    return -1;
}

/*
 * this is write back function we will use
 * to write back our metadata about the disk
 * Input:
 *      buf: data we want to write back into the disk
 *      block_offset: the index of block where we start our writing
 *      block_length: how many block we want to write
 * Return:
 *      0 if success
 *      -1 if failure
 */
int write_back(void *buf, size_t block_offset, size_t block_length)
{
    if(!buf || block_offset < 0 || block_offset >= disk.superBlock->totalBlock
        || block_offset >= disk.superBlock->totalBlock
        || block_offset + block_length >= disk.superBlock->totalBlock)
    {
        return -1;
    }

    for (size_t i = 0; i < block_length; ++i) {
        block_write(block_offset, buf);
        ++block_offset;
        buf = (char *)buf + BLOCK_SIZE;
    }

    return 0;
}

int fs_create(const char *filename)
{
    if(!disk.superBlock || disk.freeRootEntries <= 0
        || check_filename(filename) || !check_file_exist(filename))
    {
        return -1;
    }

    int fileID = get_first_free_entry();
    strcpy(disk.rootDir[fileID].filename, filename);
    disk.rootDir[fileID].size = 0;
    disk.rootDir[fileID].startIndex = FAT_EOC;

    --disk.freeRootEntries;

    assert(!write_back(disk.rootDir, disk.superBlock->rootIndex, 1));
	return 0;
}

/*
 * check if @filename is opened
 *
 * If it is opened, return 0.
 * If it is not opened, return -1.
 */
int check_file_open(const char *filename)
{
    for (int i = 0; i < FS_OPEN_MAX_COUNT; ++i){
        if(disk.FDT[i].fileID == -1)
            continue;

        int fid = disk.FDT[i].fileID;
        if(strcmp(disk.rootDir[fid].filename, filename) == 0)
            return 0;
    }
    return -1;
}

int get_file_ID(const char *filename)
{
    int fileID;
    for (fileID = 0; fileID < FS_FILE_MAX_COUNT; ++fileID) {
        if(strcmp(disk.rootDir[fileID].filename, filename) == 0)
            break;
    }
    return fileID;
}

int fs_delete(const char *filename)
{
    if(!disk.superBlock || check_filename(filename)
        || check_file_exist(filename) || !check_file_open(filename))
    {
        return -1;
    }

    //get index of @filename in root directory
    int fileID = get_file_ID(filename);
    assert(fileID < FS_FILE_MAX_COUNT);

    //empty root directory entry
    disk.rootDir[fileID].filename[0] = '\0';

    //free FAT entries
    uint16_t next = disk.rootDir[fileID].startIndex;
    uint16_t tmp;
    while(next != FAT_EOC){
        tmp = disk.arrFAT[next];
        disk.arrFAT[next] = 0;
        next = tmp;
        ++disk.freeFATEntries;
    }

    ++disk.freeRootEntries;

    assert(!write_back(disk.rootDir, disk.superBlock->rootIndex, 1));
    assert(!write_back(disk.arrFAT, 1, disk.superBlock->numFATBlock));
    return 0;
}


int fs_ls(void)
{
	if(!disk.superBlock)
	    return -1;

	printf("FS LS:\n");
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if(disk.rootDir[i].filename[0] == '\0')
            continue;
        printf("file: %s, size: %d, data_blk: %d\n",
                disk.rootDir[i].filename, disk.rootDir[i].size, disk.rootDir[i].startIndex);
    }
    return 0;
}

/*
 * we read file @fileID from virtual disk to
 * our program's memory, which makes it easy
 * for us to manipulate that file. We guarantee
 * that @fileID exist in the root directory.
 *
 * We should return fb_t that buffer all cotents
 * of the file and its count should be 1
 *
 * Note that the file we want to read can have
 * a size of 0.
 */
fb_t buffer_file(int fileID)
{
    fb_t bufferedFile = malloc(sizeof(fb));
    bufferedFile->count = 1;

     //special case: if file has the size of 0
    if(disk.rootDir[fileID].size == 0){
        bufferedFile->base = NULL;
        return bufferedFile;
    }

     //general case
    uint32_t numBlock = (disk.rootDir[fileID].size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    bufferedFile->base = malloc(numBlock * BLOCK_SIZE);
    uint16_t blockIndex = disk.rootDir[fileID].startIndex;
    int i = 0;
    while(blockIndex != FAT_EOC){
        block_read(blockIndex, (char*)bufferedFile->base + i*BLOCK_SIZE);
        blockIndex = disk.arrFAT[blockIndex];
        ++i;
    }
    assert(i == numBlock - 1);
    return bufferedFile;
}

int fs_open(const char *filename)
{
    if(!disk.superBlock || check_filename(filename)
        || check_file_exist(filename) || !disk.freeFd)
    {
        return -1;
    }

    int fileID = get_file_ID(filename);
    assert(fileID < FS_FILE_MAX_COUNT);

    //get fd and check if this file is opened before
    int fd = -1, prevFd = -1;
    for (int i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
        //get first available entry
        if(disk.FDT[i].fileID == -1) {
            fd = (fd == -1) ? i : fd;
            continue;
        }
        //for entries that is busy
        if(disk.FDT[i].fileID == fileID){
            //it means the file we want to open is already opened before
            prevFd = i;
        }
    }

    assert(!disk.FDT[fd].file && !disk.FDT[fd].offset && disk.FDT[fd].fileID == -1);

    //initialize file descriptor
    disk.FDT[fd].fileID = fileID;
    if(prevFd != -1){
        disk.FDT[fd].file = disk.FDT[prevFd].file;
        ++disk.FDT[fd].file->count;
    } else {
        disk.FDT[fd].file = buffer_file(fileID);
    }
    disk.FDT[fd].offset = disk.FDT[fd].file->base;

    --disk.freeFd;
    return fd;
}

/*
 * check if input fd is valid or not
 * return -1 if fd is invalid
 * return 0 if fd is valid
 */
int check_fd(int fd)
{
    if(fd < 0 || fd >= FS_OPEN_MAX_COUNT || disk.FDT[fd].fileID == -1)
        return -1;
    return 0;
}

int fs_close(int fd)
{
    if(!disk.superBlock || check_fd(fd))
        return -1;

    disk.FDT[fd].offset = NULL;
    --(disk.FDT[fd].file->count);

    //if count == 0, it means that there is no fd that is associated
    //with the file, we should clear the buffer
    if(!(disk.FDT[fd].file->count)){
        //we don't need write back function, because whenever
        //we change the cotent of a file, we will immediately
        //write it back to virtual disk
        free(disk.FDT[fd].file->base);
        free(disk.FDT[fd].file);
    }
    disk.FDT[fd].file = NULL;
    disk.FDT[fd].fileID = -1;

    ++disk.freeFd;
    return 0;
}

int fs_stat(int fd)
{
	if(!disk.superBlock || check_fd(fd))
	    return -1;
	int fileID = disk.FDT[fd].fileID;
    return disk.rootDir[fileID].size;
}

int fs_lseek(int fd, size_t offset)
{
    if(!disk.superBlock || check_fd(fd))
        return -1;
    //check if offset is out of bound
    int fileID = disk.FDT[fd].fileID;
    if(offset > disk.rootDir[fileID].size)
        return -1;
    //set offset
    disk.FDT[fd].offset = (char*)disk.FDT[fd].file->base + offset;
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    return 0;
}

