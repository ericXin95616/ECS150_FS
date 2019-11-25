//
// Created by bochao on 11/21/19.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fs.h>
#include <assert.h>
#include <stdint.h>
#include <disk.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <zconf.h>

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
char filenames[FS_OPEN_MAX_COUNT][FS_FILENAME_LEN];
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
 * this is the helping function of stest_read_write
 */
void read_after_write(int myfd, char *filename)
{
    //this code from Professor Porquet
    int fd = open(filename, O_RDONLY);
    struct stat st;
    if (fd < 0)
        die_perror("open");
    if (fstat(fd, &st))
        die_perror("fstat");
    if (!S_ISREG(st.st_mode))
        die("Not a regular file: %s\n", filename);

    /* Map file into buffer */
    void *write_buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!write_buf)
        die_perror("mmap");

    //use our own file system to do write and read
    assert(fs_write(myfd, write_buf, st.st_size) >= 0);

    void *read_buf = malloc(fs_stat(myfd));
    fs_lseek(myfd, 0);
    assert(fs_read(myfd, read_buf, fs_stat(myfd)) == fs_stat(myfd));

    //compare if read memory is the same as write memory
    assert(!memcmp(write_buf, read_buf, fs_stat(myfd)));

    munmap(write_buf, st.st_size);
    free(read_buf);
    close(fd);
}

/*
 * test case:
 * 1, read/write before open
 * 2, read/write after close
 * 3, invalid input fd
 * 4. buf is null and count is 0?
 * 5, read/write with offset does not align with
 * the beginning of the block
 * 6, read/write across multiple blocks
 * 7, read when readable byte is smaller than @count
 * 8, write pass the end of the file
 * 9, write when there is not enough space in the disk
 *
 * Note that the last test case may be better tested
 * by expanding test_fs_student.sh
 */
void stest_read_write_stat(void)
{
    fs_mount(diskname);
    for (int i = 0; i < num_input_files; ++i) {
        fs_create(filenames[i]);
    }

    //case 1
    void *buf = malloc(FS_FILENAME_LEN);
    assert(fs_write(0, buf, FS_FILENAME_LEN) == -1);

    //create all the input files
    for (int m = 0; m < num_input_files; ++m) {
        assert(!fs_create(filenames[m]));
    }
    //open all the input files
    int fd[num_input_files];
    for (int j = 0; j < num_input_files; ++j) {
        fd[j] = fs_open(filenames[j]);
        assert(fd[j] != -1);
    }

    //case 3 and 4
    assert(fs_write(-1, buf, FS_FILENAME_LEN) == -1);
    assert(fs_write(FS_OPEN_MAX_COUNT, buf, FS_FILENAME_LEN) == -1);
    assert(!fs_write(fd[0], NULL, 0));

    //case 5-9
    for (int l = 0; l < num_input_files; ++l) {
        read_after_write(fd[l], filenames[l]);
    }

    //close all the input files
    for (int k = 0; k < num_input_files; ++k) {
        assert(!fs_close(fd[k]));
    }

    //case 2
    assert(fs_write(0, buf, FS_FILENAME_LEN) == -1);

    free(buf);
    fs_umount();

    printf("Pass: simple test for fs_read and fs_write.\n");
}

/*
 * this is a helper function for stest_lseek
 * fd's offset is guarantee to be 0
 * we want to append a postfix into @fd
 */
void append_postfix(int fd)
{
}

/*
 * test case:
 * 1, lseek before open
 * 2, lseek after close
 * 3, invalid offset
 * 4, append something
 * 5, write something in the mid of a file
 */
void stest_lseek(void)
{
    fs_mount(diskname);

    int fd[num_input_files];
    for (int i = 0; i < num_input_files; ++i) {
        fd[i] = fs_open(filenames[i]);
    }

    for (int j = 0; j < num_input_files; ++j) {
        append_postfix(fd[j]);
    }

    for (int k = 0; k < num_input_files; ++k) {
        fs_close(fd[k]);
    }

    fs_umount();
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

    stest_read_write_stat();

    stest_lseek();
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