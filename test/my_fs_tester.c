//
// Created by bochao on 11/21/19.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fs.h>
#include <assert.h>
#include <stdint.h>

#define test_fs_error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

#define die(...)				\
do {							\
	test_fs_error(__VA_ARGS__);	\
	exit(1);					\
} while (0)

#define die_perror(msg)			\
do {							\
	perror(msg);				\
	exit(1);					\
} while (0)

char diskname[FS_FILENAME_LEN];
char filenames[FS_FILE_MAX_COUNT][FS_FILENAME_LEN];
int num_input_files;

/*
 * test cases:
 * 1, umount before mount
 * 2, umount after umount
 * 3, mount twice
 * 4, open a file and umount
 */
void stest_mount_umount()
{
    //case 1
    assert(fs_umount());

    fs_mount(diskname);

    //case 3
    assert(fs_mount(diskname));

    //case 4
    assert(!fs_create(filenames[0]));
    int fd = fs_open(filenames[0]);
    //cannot umount, because there is file still open
    assert(fs_umount());
    assert(!fd && !fs_close(fd));

    fs_delete(filenames[0]);
    fs_umount();

    //case 2
    assert(fs_umount());

    printf("Pass: simple test for fs_mount and fs_umount.\n");
}

/*
 * generate filename from "AA" to "ZZ"
 * What name will be generated depends
 * on input ord
 */
void filename_generator(char *tmp, int ord)
{
    int first = ord / 26;
    int second = ord % 26;
    tmp[0] = 'A' + first;
    tmp[1] = 'A' + second;
    tmp[2] = '\0';
}
/*
 * test cases:
 * 1, create/delete before mount
 * 2, create/delete after umount
 * 3, invalid input filename
 * 4, create: filename already exist
 *    delete: filename does not exist
 * 5, create when disk root directory is full
 * 6. delete when current file name is opened
 */
void stest_create_delete()
{
    //case 1
    assert(fs_create(filenames[0]) && fs_delete(filenames[0]));

    fs_mount(diskname);

    //case 3
    //filename is NULL
    assert(fs_create(NULL));
    char invalid_filename[FS_FILENAME_LEN];
    //first char is NULL char
    invalid_filename[0] = '\0';
    assert(fs_create(invalid_filename));
    //not null-terminated
    for (int i = 0; i < FS_FILENAME_LEN; ++i)
        invalid_filename[i] = 'a';
    assert(fs_create(invalid_filename));

    //case 4
    assert(fs_delete(filenames[0]));
    assert(!fs_create(filenames[0]));
    assert(fs_create(filenames[0]));
    assert(!fs_delete(filenames[0]));

    //case 5
    char *tmp = malloc(FS_FILENAME_LEN);
    for (int j = 0; j < FS_FILE_MAX_COUNT; ++j) {
        filename_generator(tmp, j);
        assert(!fs_create(tmp));
    }
    assert(fs_create(filenames[0]));
    for (int j = 0; j < FS_FILE_MAX_COUNT; ++j) {
        filename_generator(tmp, j);
        assert(!fs_delete(tmp));
    }

    //case 6
    assert(!fs_create(filenames[0]));
    int fd = fs_open(filenames[0]);
    assert(!fd);
    //should fail to delete because it is opened
    assert(fs_delete(filenames[0]));
    assert(!fs_close(fd));
    assert(!fs_delete(filenames[0]));

    fs_umount();

    //case 2
    assert(fs_create(filenames[0]) && fs_delete(filenames[0]));

    printf("Pass: simple test for fs_create and fs_delete.\n");
}

/*
 * test cases:
 * 1, open/close before mount
 * 2, open/close after umount
 * 3, open/close before create
 * 4, open/close after delete
 * 5, close before open
 * 6, close after close
 * 7, open multiple times, check if fd is different
 * 8, open when FDT is full
 * 9, close with invalid fd
 *
 * Note: I don't test invalid filename input here,
 * because I use same function to check if filename
 * is valid or not. If it passes stest_create_delete,
 * it is not necessary to test it here
 */
void stest_open_close()
{
    //case 1
    int fd = fs_open(filenames[0]);
    assert(fd == -1 && fs_close(fd));

    fs_mount(diskname);

    //case 3
    fd = fs_open(filenames[0]);
    assert(fd == -1 && fs_close(fd));

    fs_create(filenames[0]);

    //case 5
    assert(fs_close(0));

    //case 7
    fd = fs_open(filenames[0]);
    int tmp;
    for (int i = 1; i < FS_OPEN_MAX_COUNT; ++i) {
        tmp = fd;
        fd = fs_open(filenames[0]);
        assert(fd == tmp + 1);
    }

    //case 8, cannot open anymore file because FDT is full
    assert(fs_open(filenames[0]) == -1);

    for (int j = 0; j < FS_OPEN_MAX_COUNT; ++j)
        assert(!fs_close(j));

    //case 6, cannot close because all files have been closed
    assert(fs_close(fd));

    //case 9
    fd = fs_open(filenames[0]);
    assert(fs_close(-1));
    assert(fs_close(INT8_MAX));
    assert(fs_close(FS_OPEN_MAX_COUNT));
    assert(!fs_close(fd));

    fs_delete(filenames[0]);

    //case 4
    fd = fs_open(filenames[0]);
    assert(fd == -1 && fs_close(fd));

    fs_umount();

    //case 2
    fd = fs_open(filenames[0]);
    assert(fd == -1 && fs_close(fd));

    printf("Pass: simple test for fs_open and fs_delete.\n");
}

/*
 * test case:
 * 1, read/write before open
 * 2, read/write after close
 * 3, invalid input fd
 * 4, read/write with offset does not align with
 * the beginning of the block
 * 5, read/write across multiple blocks
 * 6, read when readable byte is smaller than @count
 * 7, write pass the end of the file
 * 8, write when there is not enough space in the disk
 *
 * Note that the last test case may be better tested
 * by expanding test_fs_student.sh
 */
void stest_read_write(void)
{
    
}

void stest_seek_stat(void)
{

}
/*
 * this is the simple test of file system
 * in every test cases, we guarantee that
 * the cotent of disk and files will not
 * change before and after each test call
 */
void simple_test(void)
{
    stest_mount_umount();

    stest_create_delete();

    stest_open_close();

    stest_read_write();

    stest_seek_stat();
}

int main(int argc, char *argv[])
{
    if(argc < 3)
        die("Incorrect input. Need: <diskname> <filenames>");

    --argc;
    ++argv;

    strcpy(diskname, argv[0]);
    printf("%s\n", diskname);
    for (int i = 1; i < argc; ++i) {
        strcpy(filenames[i-1], argv[i]);
        printf("%s\n",filenames[i]);
    }
    num_input_files = --argc;

    simple_test();
}