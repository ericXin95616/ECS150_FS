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

#define BLOCK_NUM(a) ((a + BLOCK_SIZE - 1)/BLOCK_SIZE)
#define die_perror(msg)			\
do {							\
	perror(msg);				\
	exit(1);					\
} while (0)

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

//whenever we create a file descriptor
//we will buffer data of that file
typedef struct file_descriptor{
    int fileID;
    size_t offset;
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
	    die_perror("malloc");
	}
	block_read(0, superBlock);

	//error-checking super block
	uint16_t correctFATBlock = BLOCK_NUM(2 * superBlock->numDataBlock);
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
	    die_perror("malloc");
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
        die_perror("malloc");
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
        die_perror("malloc");
    }
    for (int l = 0; l < FS_OPEN_MAX_COUNT; ++l){
        //we use -1 indicates that entry is free
        FDT[l].fileID = -1;
        FDT[l].offset = 0;
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
    int fd;
    for (fd = 0; fd < FS_OPEN_MAX_COUNT; ++fd) {
        //get first available entry
        if(disk.FDT[fd].fileID == -1)
            break;
    }

    assert(disk.FDT[fd].offset == 0 && disk.FDT[fd].fileID == -1);

    //initialize file descriptor
    disk.FDT[fd].fileID = fileID;

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

    disk.FDT[fd].fileID = -1;
    disk.FDT[fd].offset = 0;

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
    disk.FDT[fd].offset = offset;
    return 0;
}

/*
 * These flags are used to helps us manage the behavior of fs_read and fs_write.
 *
 * BLOCK_END means offset of file is reaching the end of the buffered
 * block. Therefore, we need to read/write to a new block
 *
 * FILE_END means offset of file is reaching the end of the file.
 * For read operation, we need to stop reading and return the number
 * of bytes we actually read.
 * For write operation, we need to allocate new block in virtual disk
 * for the file so that we can continue to write to the disk. If no space
 * in the disk, write should be stopped and return the number of bytes we
 * actually write.
 *
 * OP_END means the read/write operation ends normally, we need to
 * return @count bytes.
 */
typedef enum end_flag{BLOCK_END, FILE_END, OP_END} end_flag;
typedef enum operation{READ, WRITE} OP;

/*
 * return the index of block where
 * offset of @fd is located. We
 * guarantee that fd is valid
 */
uint16_t get_offset_block(int fd)
{
    int fileID = disk.FDT[fd].fileID;
    assert(disk.FDT[fd].offset >= 0 && disk.FDT[fd].offset <= disk.rootDir[fileID].size);

    uint16_t blockIndex = disk.rootDir[fileID].startIndex;
    size_t numBlock = disk.FDT[fd].offset / BLOCK_SIZE;

    for (size_t i = 0; i < numBlock; ++i) {
        blockIndex = disk.arrFAT[blockIndex];
    }
    assert(blockIndex != FAT_EOC);
    return disk.superBlock->dataStartIndex + blockIndex;
}

/*
 * This function is the key to our fs_write and fs_read!
 * return which end will come next
 * @fd: File descriptor
 * @count: Number of bytes needed to read
 */
end_flag next_end(int fd, size_t count)
{
    int fileID = disk.FDT[fd].fileID;
    assert(disk.FDT[fd].offset >= 0 && disk.FDT[fd].offset <= disk.rootDir[fileID].size);

    //TODO: +1 or not +1? This is a problem
    size_t byteToBlockEnd = BLOCK_SIZE - (disk.FDT[fd].offset) % BLOCK_SIZE;
    size_t byteToFileEnd = disk.rootDir[fileID].size - disk.FDT[fd].offset;

    if(count < byteToBlockEnd){
        if(count < byteToFileEnd)
            return OP_END;
        return FILE_END;
    } else {
        if(byteToBlockEnd < byteToFileEnd)
            return BLOCK_END;
        return FILE_END;
    }
}

/*
 * return cache offset given fd
 */
size_t get_cache_offset(int fd)
{
    int fileID = disk.FDT[fd].fileID;
    assert(disk.FDT[fd].offset >= 0 && disk.FDT[fd].offset <= disk.rootDir[fileID].size);

    size_t cache_offset = (disk.FDT[fd].offset) % BLOCK_SIZE;
    return cache_offset;
}

/*
 * @fd: File descriptor
 * @count: Number of bytes of data to be written
 * it will update the size of the file that fd
 * associated with.
 * If offset + count <= size, size of the file will no change
 * If offset + count > size, size of the file will change
 *
 * Return: the size of the file
 */
size_t update_file_size(int fd, size_t count)
{
    int fileID = disk.FDT[fd].fileID;
    if(disk.FDT[fd].offset + count <= disk.rootDir[fileID].size)
        return disk.rootDir[fileID].size;
    return disk.FDT[fd].offset + count;
}

/*
 * @fd: File descriptor
 * @count: Number of blocks need to be allocate
 *
 * Return: Number of blocks that are actually allocated
 */
size_t get_new_block(int fd, size_t count)
{
    int fileID = disk.FDT[fd].fileID;
    uint16_t blockIndex = disk.rootDir[fileID].startIndex;

    //move to the end of the linked list
    while(blockIndex != FAT_EOC && disk.arrFAT[blockIndex] != FAT_EOC)
        blockIndex = disk.arrFAT[blockIndex];

    size_t blockAllocated = 0;
    //disk.arrFAT[0] is always FAT_EOC, so we check from the second element
    for (int i = 1; i < disk.superBlock->numDataBlock; ++i) {
        if(blockAllocated >= count)
            break;
        if(disk.arrFAT[i] != 0)
            continue;
        //disk.arrFAT[i] == 0 and blockAllocated < count
        ++blockAllocated;
        if(blockIndex == FAT_EOC){
            blockIndex = i;
            disk.rootDir[fileID].startIndex = i;
            continue;
        }
        disk.arrFAT[blockIndex] = i;
        blockIndex = i;
    }
    disk.arrFAT[blockIndex] = FAT_EOC;
    disk.freeFATEntries -= blockAllocated;
    return blockAllocated;
}

/*
 * operate = either write or read
 * @fd: File descriptor
 * @buf: Data buffer to operate
 * @buf_offset: Which data we want to operate next
 * @count: Total number of data we want to operate
 * @blockIndex: which data block we want to operate
 * @cache_offset: Where we want to operate in disk
 * @flag: Which end will come next
 *
 * This function is meant to deal with mismatch write and read.
 * If offset is not aligned to the beginning of the block
 * Or end does not aligned to the end of the block, this
 * function will be called.
 *
 * This function will not change anything to outside variable,
 * except writing to disk.
 *
 * Return: Number of bytes that are actually operated
 */
size_t mismatch_write_read(int fd, void *buf, size_t buf_offset, size_t count, uint16_t blockIndex,
                      size_t cache_offset, end_flag flag, OP operation)
{
    if(count == buf_offset)
        return 0;

    void *cache = malloc(BLOCK_SIZE);
    if (!cache)
        die_perror("malloc");
    block_read(blockIndex, cache);

    //Calculate how many bytes we need to operate
    size_t opByte;
    int fileID = disk.FDT[fd].fileID;
    if(flag == BLOCK_END) {
        opByte = BLOCK_SIZE - cache_offset;
    } else if (flag == FILE_END){
        opByte = disk.rootDir[fileID].size - disk.FDT[fd].offset;
    } else {
        opByte = count;
    }

    //based on operation, we decide what's dest and what's src
    void *dest;
    void *src;
    if(operation == WRITE){
        dest = (char *)cache + cache_offset;
        src = (char *)buf + buf_offset;
    } else {
        dest = (char *)buf + buf_offset;
        src = (char *)cache + cache_offset;
    }

    memcpy(dest, src, opByte);

    //if operation is write, we need to write back to disk
    if(operation == WRITE)
        block_write(blockIndex, cache);

    free(cache);

    return opByte;
}

/*
 * @fd: File descriptor
 * @buf: Data buffer
 * @count: Number of bytes
 * @operation: Which operation need to be performed
 *
 * this function is meant to combine the common part
 * of fs_read and fs_write, so that it will be easier
 * for us to debug or add new function.
 *
 * Return: the actual byte that is being read or written
 */
size_t disk_write_read(int fd, void *buf, size_t count, OP operation)
{
    //these are set up work
    size_t buf_offset = 0;
    size_t old_val_offset = disk.FDT[fd].offset;
    size_t cache_offset = get_cache_offset(fd);
    size_t opByte = 0;
    uint16_t blockIndex = 0;
    end_flag flag = next_end(fd, count);

    while(flag == BLOCK_END)
    {
        blockIndex = get_offset_block(fd);
        //read next block
        if(cache_offset > 0){
            opByte = mismatch_write_read(fd, buf, buf_offset, count, blockIndex, cache_offset, flag, operation);
        } else {
            if(operation == WRITE)
                assert(!block_write(blockIndex, (char *)buf + buf_offset));
            else
                assert(!block_read(blockIndex, (char *)buf + buf_offset));
            opByte = BLOCK_SIZE;
        }

        //update all variable accordingly
        buf_offset += opByte;
        disk.FDT[fd].offset += opByte;
        cache_offset = get_cache_offset(fd);
        flag = next_end(fd, count - buf_offset);
    }

    blockIndex = get_offset_block(fd);
    opByte = mismatch_write_read(fd, buf, buf_offset, count, blockIndex, cache_offset, flag, operation);
    disk.FDT[fd].offset += opByte;

    return disk.FDT[fd].offset - old_val_offset;
}

int fs_write(int fd, void *buf, size_t count)
{
    if(!disk.superBlock || check_fd(fd))
        return -1;
    if(!count)
        return 0;

    int fileID = disk.FDT[fd].fileID;

    //First, we want to check if we need to allocate new blocks.
    //If we need, we allocate them beforehand
    size_t old_val_size = disk.rootDir[fileID].size;
    size_t old_block_num = BLOCK_NUM(disk.rootDir[fileID].size);
    disk.rootDir[fileID].size = update_file_size(fd, count);
    size_t new_block_num = BLOCK_NUM(disk.rootDir[fileID].size);
    size_t get_block_num = 0;

    //allocate new blocks for @fileID
    if(new_block_num > old_block_num) {
        get_block_num = get_new_block(fd, new_block_num - old_block_num);
        assert(get_block_num <= new_block_num - old_block_num);
        if (get_block_num < new_block_num - old_block_num)
            disk.rootDir[fileID].size = (old_block_num + get_block_num) * BLOCK_SIZE;
    }

    size_t writeByte = disk_write_read(fd, buf, count, WRITE);

    //write dirty metadata back into the disk
    if(old_val_size != disk.rootDir[fileID].size)
        assert(!write_back(disk.rootDir, disk.superBlock->rootIndex, 1));
    if(get_block_num)
        assert(!write_back(disk.arrFAT, 1, disk.superBlock->numFATBlock));

    return writeByte;
}

int fs_read(int fd, void *buf, size_t count)
{
	if(!disk.superBlock || check_fd(fd))
	    return -1;
    if(!count)
        return 0;

    size_t readByte = disk_write_read(fd, buf, count, READ);

    return readByte;
}