# ECS 150 Project 4 Report

## Phase 1: Mount, Umount and Info
We used three structs to contain the basic data
structure needed by our file system. The overall
container struct is `virtualDisk`, which contains
Super Block, FAT arry, Root Directory, file 
descriptors, and numbers that keep records of
numbers of free entries in different fields.
There's also structs for Root Directory and
Super Block.
###fs_mount
First, we use the given `block_disk_open` to open
a disk of the specified name. Then we read in
the super block into our super block struct, and 
do an error check with our calculated correct
values. If the error check fails, we free the 
struct and return. In the same way, we read in
the File Allocation Table, and do an error check.
After that, we compute and initialize the free
FAT entries that were left. And then we create
and initialize the file descriptor table. 
Finally, we put everything together in the 
global variable virtual disk `disk`.
###fs_umount
Upon umount, we free every field that we put in
`disk` in `fs_mount` and return.
###fs_info
In `fs_info`, we print out the required fields
in the virtual disk that we saved as a global
variable. For the free ratio part, we calculate
it by using free entries divided by total number
of entries, so we don't have to save them in the
virtual disk struct.

## Phase 2: Create, Delete and ls
We created several helper functions for this 
phase. `check_filename` is used for checking
if the input file name is legal. `check_file_exist` 
checks if a file of the same name is already in
the root directory. `get_first_free_entry` gets
us the index of the first free entry in root 
directory for creating the file. `write_back`
uses the provided `block_write` to help us write
back the changed metadata about the disk.
`check_file_open` checks if a file of this name is
opened. `get_file_ID` gets the file id of a file
of a certain file name.
###fs_create
First, we check if the super block is valid, if the
file name is legal, if the file name isn't there yet,
or if the disk doesn't have anymore free entries. If
any of the conditions doesn't match, we return.
Then we get the first free entry in root directory,
and initialized the corresponding entry with the 
file name, size zero and start index `FAT_EOC`. 
Then we update the free root entries number. 
Finally, we write everything that we just changed 
back to the disk.
###fs_delete
First, we go through the checking process that's 
similar in `fs_create`, while also checking if
the file is already opened. Once we passed the 
checking process, we fine the index for the input
filename in the root directory. Then we clean the
correponding root directory entry and also free
up the FAT entries. After that, we update the 
number of free root entries. Finally, we write
the newly changed information of root directory
and FAT array with `write_back` back to the disk.
###fs_ls
We use a for look in root directory to find non-null
file names. If a file name is non-null, we print out
the file name, size, and its start index.

## Phase 3: Open, Close and Stat
We used one helper function `check_fd` here, which
helps us check if the input file descriptor is 
valid or not.
###`fs_open`
First we check all the conditions, including if the
super block is valid, if the file name is legal, if
the file exists, or there's enough free file 
descriptors. After checking, we find the first free
entry in the file descriptor table, make sure 
that it's empty, and initialize the file descriptor.
Finally, we update the number of free file 
descriptors.
###`fs_close`
We make sure there is a super block and the file is
opened. After that, we erase the information on 
file descriptor and update the number of free file
descriptors.
###`fs_stat`
In this function, after making sure the file is 
opend, we simply find the file id bythe index 
of file descriptor in the FDT, and then find 
the file size by the file id.
###`fs_lseek`
We check if the file is opened, and then set the
offset after making sure the offset is not larger
than it's supposed to be.

## Phase 4: Reading and Writing
First, we created two enums. The `end_flag` helps
us know what condition we are in. `BLOCK_END`
means offset of the the file is reaching the end
of the buffered block. Then we'll have to read or
write to a new block. `FILE_END` means that the
offset we are provided with is reaching the end
of the actual file. If we're reading, we should
stop at where we finished. If we are writing, 
then we need to allocate a new block for the file
so that we can continue to write to the disk.
`OP_END` means the reading or writing process
will end normally and we need to return our
count number of bytes.
The operation code `READ` and `WRITE` helped us
simplify the code since a large part of reading
 and writing are similar.
We used multiple helper functions in this phase.
`next_end` decides what condition we're in: are
we approaching the end of the buffered block,
approaching the end of the actual file, or the
operation is going to end normally? This function
assigns the `end_flag` accordingly.
`get_cache_offset` gets the position of offset
in a single block.
In `get_offset_block`, we find the index of the
block that holds the offset of a file opened in 
a certain file descriptor fd by following the 
linked list in array of FAT.
`get_new_block` finds as many empty blocks as 
needed in array of FAT for the writing task, 
which will be discussed later.
###`disk_write_read`
This function combined the common part of fs_read
and fs_write. In this function, we read/write
(depends on the opcode) block by block with 
`block_write` and `block_read`. if theoffset/end 
of the file doesn't match the beginning/
end of a block exactly, we use the 
`mismatch_write_read` to deal with that part.
----------------------------------
|   blk1   |   blk2   |    blk3  |
|          |          |          |
|	 |-----|----------|-----|    |
|____|_____|__________|_____|____|
 off |     |          |     |  end
     |-----|----------|-----|
     mismatch          mismatch
###`mismatch_write_read`
First we do the validation check, and then we
initialize a block cache and read the block into
this cache. Then we calculate how many bytes
we need to read/ write. Then, we set the source and
destination base on the op code and add offset to
those addresses. After that, we do a `memcpy` finish
the actual data transfer. In the end, if we are 
writing, we need to write the write the dirty block
back to the disk with `block_write`.
###`fs_read`
We first check if the file is opened, or if the
file size is not zero. After all conditions are
satisfied, we use the `disk_write_read` function
with operation code `READ`.
###`fs_write`
After the validation check, we first calculate 
if this writing requires allocation of new blocks.
If so, we allocte the new blocks with 
`get_new_block` before doing anythng else. Then,
we call `disk_write_read` with op code `WRITE`
to deal with the actual writing. Finally, we update
the changed metadata back into the disk with the 
`write_back` function that we implemented earilier.

##Testing

