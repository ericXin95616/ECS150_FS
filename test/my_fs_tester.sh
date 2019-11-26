#!/bin/bash

readonly FS_OPEN_MAX=32

# make virtual disk with 4096 blocks
echo -e "\n--- Creating Disk ---\n"
./fs_make.x disk.fs 4096
echo -e "\n--- Disk Information ---\n"
./fs_ref.x info disk.fs
./fs_ref.x ls disk.fs

# generate 32 files with random size
echo -e "\n--- Creating Test Files ---\n"
declare -a arr=("test-file-1"
                "test-file-2"
                "test-file-3"
                "test-file-4"
                "test-file-5"
                "test-file-6"
                "test-file-7"
                "test-file-8"
                "test-file-9"
                "test-file-10"
                "test-file-11"
                "test-file-12"
                "test-file-13"
                "test-file-14"
                "test-file-15"
                "test-file-16"
                "test-file-17"
                "test-file-18"
                "test-file-19"
                "test-file-20"
                "test-file-21"
                "test-file-22"
                "test-file-23"
                "test-file-24"
                "test-file-25"
                "test-file-26"
                "test-file-27"
                "test-file-28"
                "test-file-29"
                "test-file-30"
                "test-file-31"
                "test-file-32"
)

for str in "${arr[@]}"
do
  dd if=/dev/urandom of=$str count=10 bs=$RANDOM
done

# input those file to my_fs_tester
echo -e "\n--- Run Tester ---\n"
./my_fs_tester.x disk.fs "${arr[@]}"
echo -e "\n--- Test Result ---\n"
./fs_ref.x info disk.fs
./fs_ref.x ls disk.fs

rm disk.fs
rm "${arr[@]}"