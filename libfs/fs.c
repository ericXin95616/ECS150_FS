#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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
    uint16_t dataBlock;
    uint8_t FATBlock;
    int8_t unused[4079];
}sBlock;

typedef sBlock* sBlock_t;

typedef struct __attribute__((__packed__)) entryOfRootDirectory{
    char filename[16];
    uint32_t size;
    uint16_t startIndex;
    int8_t unused[10];
}fileInfo;


typedef struct virtualDisk{
    sBlock_t superBlock;
    uint16_t *arrFAT;
    fileInfo rootDir[ROOT_DIR_SIZE];
}vDisk;

static vDisk disk = {.superBlock = NULL,.arrFAT = NULL};

int fs_mount(const char *diskname)
{
	if(block_disk_open(diskname))
        return -1;

	sBlock_t superBlock = malloc(sizeof(superBlock));
	if(!superBlock){
	    perror("malloc");
	    return -1;
	}
	block_read(0, superBlock);

	//error-checking super block
	uint16_t correctFATBlock = (2 * superBlock->dataBlock + BLOCK_SIZE - 1) / BLOCK_SIZE;
	uint16_t correctDataBlock = block_disk_count() - correctFATBlock - 2;
	if(strcmp(superBlock->signature, SIGNATURE) != 0
	    || superBlock->totalBlock != block_disk_count()
	    || superBlock->FATBlock != correctFATBlock
	    || superBlock->dataBlock != correctDataBlock
	    || superBlock->rootIndex != correctFATBlock + 1
	    || superBlock->dataStartIndex != superBlock->rootIndex + 1)
    {
	    free(superBlock);
	    fs_error("Super block does not have the expected format.\n");
	    return -1;
    }

	//arrFAT should have the same size as the total FATBlock
	uint16_t *arrFAT = malloc(superBlock->FATBlock * BLOCK_SIZE);
    for (int i = 0; i < superBlock->FATBlock; ++i)
        block_read(i + 1, (char*)arrFAT + i * BLOCK_SIZE);

    //error check for FAT
    if(arrFAT[0] != FAT_EOC){
        free(superBlock);
        free(arrFAT);
        fs_error("FAT does not have the expected format.\n");
        return -1;
    }

    block_read(superBlock->rootIndex, disk.rootDir);

    //error check for root directory
    
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

