#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define ROOT_DIR_SIZE 128
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

typedef struct virtualDisk{
    sBlock_t superBlock;
    uint16_t *arrFAT;
    fileInfo_t rootDir;
    int freeFATEntries;
    int freeRootEntries;
}vDisk;

static vDisk disk = {.superBlock = NULL,
                        .arrFAT = NULL,
                        .rootDir = NULL};

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
	    fs_error("Super block does not have the expected format.\n");
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
        fs_error("FAT does not have the expected format.\n");
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
    for (int j = 0; j < ROOT_DIR_SIZE; ++j) {
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
        fs_error("Root directory does not have the expected format.\n");
        return -1;
    }

    //initialize global variable disk
    disk.superBlock = superBlock;
    disk.arrFAT = arrFAT;
    disk.rootDir = rootDir;
    disk.freeFATEntries = freeFATEntries;
    disk.freeRootEntries = freeRootEntries;

    return 0;
}

int fs_umount(void)
{
    //TODO: return -1 if there are still open file descriptor
    //no virtual disk is opened or disk close fail
    if(!disk.superBlock || block_disk_close())
        return -1;
    //free everything and quit
    free(disk.superBlock);
    free(disk.arrFAT);
    free(disk.rootDir);
    disk.superBlock = NULL;
    disk.arrFAT = NULL;
    disk.rootDir = NULL;
    return 0;
}

int fs_info(void)
{
	if(!disk.superBlock)
        return -1;
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", disk.superBlock->totalBlock);
	printf("fat_blk_count=%d\n", disk.superBlock->numFATBlock);
	printf("rdir_blk=%d\n", disk.superBlock->rootIndex);
	printf("data_blk=%d\n", disk.superBlock->dataStartIndex);
	printf("data_blk_count=%d\n", disk.superBlock->numDataBlock);
	printf("fat_free_ratio=%d/%d\n", disk.freeFATEntries, disk.superBlock->numDataBlock);
	printf("rdir_free_ratio=%d/%d\n", disk.freeRootEntries, ROOT_DIR_SIZE);
    return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
    return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
    return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
    return 0;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
    return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
    return 0;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
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

