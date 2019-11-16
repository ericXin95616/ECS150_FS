#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF

//SB is 4096 bytes
typedef struct Superblock{
  uint8_t signature[8];
  uint16_t blocks_num;
  uint16_t rootdir_idx;
  uint16_t dbstart_idx;
  uint16_t db_num;
  uint8_t FATblocks_num;
  uint8_t padding[4079];
}__attribute__ ((packed)) Superblock;

//RE is 32 bytes
typedef struct Root_entry{
  uint8_t filename[16];
  uint32_t filesize;
  uint16_t firstdb_idx;
  uint8_t padding[10];
}__attribute__ ((packed)) Root_entry;

//Rootdir is 32*128 bytes
typedef struct Root_dir{
  Root_entry Root_entries[128];
}__attribute__ ((packed)) Root_dir;

//global variables
Superblock *SB;
uint16_t *FAT;
Root_dir *Root;



int fs_mount(const char *diskname)
{
  //open the disk using the block api in disk.h
	if(block_disk_open(diskname) == -1) {return -1;}

  //load the meta-information and checking

  //Mounting Superblock
  SB = malloc(sizeof(Superblock));
  if(SB == NULL) {
    return -1;
  }
  if(block_read(0,SB) ==-1) {
    free(SB);
    return -1;
  }
  //checking SB
  //sizeFat = db_num*2
  //blockFat = db_num*2/4096
  if(memcmp(SB->signature,"ECS150FS",8)!=0
     || SB->blocks_num!=block_disk_count()
     || SB->FATblocks_num!=(SB->db_num)*2/4096
     || SB->rootdir_idx!=SB->FATblocks_num+1
     || SB->dbstart_idx!=SB->rootdir_idx+1
     || SB->db_num!=SB->blocks_num-2-SB->FATblocks_num){
    free(SB);
    return -1;
  }

  //Mounting FAT
  FAT = malloc(SB->db_num*sizeof(uint16_t));
  for(int i=0; i<SB->FATblocks_num; i++){
    if(block_read(1+i, FAT+(i*4096/2))==-1){
      return -1;
    }//Pointer arithmetic moves by the size of the object being pointed to, in this case, 1 uint16_t is 2 bytes, thus 4096/2 integers
  }
  //checking FAT
  if(FAT[0]!=FAT_EOC){
    free(SB);
    free(FAT);
    return -1;
  }

  //Mounting Root_dir
  Root = malloc(sizeof(Root_dir));
  if(block_read(SB->rootdir_idx, Root)==-1){
    free(SB);
    free(FAT);
    free(Root);
    return -1;
  }

  return 0;
}

int fs_umount(void)
{

  return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
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
