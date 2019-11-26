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
echo -e "\n--- Test 1: my unit test ---\n"
./my_fs_tester.x disk.fs "${arr[@]}"
echo -e "\n--- Test Result ---\n"
./fs_ref.x info disk.fs
./fs_ref.x ls disk.fs

# begin test 2
echo -e "\n--- Creating Disk ---\n"
./fs_make.x ref_disk.fs 4096
./fs_make.x test_disk.fs 4096

# test begins
echo -e "\n--- Test 2: general case ---\n"

touch ref.stdout ref.stderr test.stdout test.stderr
REF_STDOUT=$(cat ref.stdout)
REF_STDERR=$(cat ref.stderr)
REF_DISK=$(xxd ref_disk.fs)
TEST_STDOUT=$(cat test.stdout)
TEST_STDERR=$(cat test.stderr)
TEST_DISK=$(xxd test_disk.fs)

for str in "${arr[@]}"
do
  echo -e "\n--- $str ---\n"
  {
    ./fs_ref.x add ref_disk.fs $str
    ./fs_ref.x info ref_disk.fs
    ./fs_ref.x ls ref_disk.fs
    ./fs_ref.x stat ref_disk.fs $str
    ./fs_ref.x cat ref_disk.fs $str
  } >>ref.stdout 2>>ref.stderr

  {
    ./test_fs.x add test_disk.fs $str
    ./test_fs.x info test_disk.fs
    ./test_fs.x ls test_disk.fs
    ./test_fs.x stat test_disk.fs $str
    ./test_fs.x cat test_disk.fs $str
  } >>test.stdout 2>>test.stderr

  # compare stdout
  if [ "$REF_STDOUT" != "$TEST_STDOUT" ]; then
    echo "Stdout outputs don't match..."
    diff -u ref.stdout test.stdout
  else
    echo "Stdout outputs match!"
  fi
  # compare stderr
  if [ "$REF_STDERR" != "$TEST_STDERR" ]; then
    echo "Stderr outputs don't match..."
    diff -u ref.stderr test.stderr
  else
    echo "Stderr outputs match!"
  fi
  # compare disk
  if [ "$REF_DISK" != "$TEST_DISK" ]; then
    echo "Disks' contents don't match..."
  else
    echo "Disks' contents match!"
  fi
done

# Test 3: corner case, disk does not have enough space
echo -e "\n--- Creating Disk ---\n"
./fs_make.x ref_small_disk.fs 1
./fs_make.x test_small_disk.fs 1

echo -e "\n--- Creating File ---\n"
dd if=/dev/urandom of=test-file-0 count=8 bs=1024

REF_SMALL_DISK=$(xxd ref_small_disk.fs)
TEST_SMALL_DISK=$(xxd test_small_disk.fs)

echo -e "\n--- Test 3: disk does not have enough space ---\n"
{
  ./fs_ref.x add ref_small_disk.fs test-file-0
  ./fs_ref.x info ref_small_disk.fs
  ./fs_ref.x ls ref_small_disk.fs
  ./fs_ref.x stat ref_small_disk.fs test-file-0
  ./fs_ref.x cat ref_small_disk.fs test-file-0
} >ref.stdout 2>ref.stderr

{
  ./test_fs.x add test_small_disk.fs test-file-0
  ./test_fs.x info test_small_disk.fs
  ./test_fs.x ls test_small_disk.fs
  ./test_fs.x stat test_small_disk.fs test-file-0
  ./test_fs.x cat test_small_disk.fs test-file-0
} >test.stdout 2>test.stderr

# compare stdout
if [ "$REF_STDOUT" != "$TEST_STDOUT" ]; then
  echo "Stdout outputs don't match..."
  diff -u ref.stdout test.stdout
else
  echo "Stdout outputs match!"
fi
# compare stderr
if [ "$REF_STDERR" != "$TEST_STDERR" ]; then
  echo "Stderr outputs don't match..."
  diff -u ref.stderr test.stderr
else
  echo "Stderr outputs match!"
fi
# compare disk
if [ "$REF_SMALL_DISK" != "$TEST_SMALL_DISK" ]; then
  echo "Disks' contents don't match..."
else
  echo "Disks' contents match!"
fi

# Test 4: load something that is not disk
echo -e "\n--- Test 4: load something that is not a disk ---\n"
./fs_ref.x info test-file-0 >ref.stdout 2>ref.stderr
./test_ref.x info test-file-0 >test.stdout 2>test.stderr
# compare stdout
if [ "$REF_STDOUT" != "$TEST_STDOUT" ]; then
  echo "Stdout outputs don't match..."
  diff -u ref.stdout test.stdout
else
  echo "Stdout outputs match!"
fi
# compare stderr
if [ "$REF_STDERR" != "$TEST_STDERR" ]; then
  echo "Stderr outputs don't match..."
  diff -u ref.stderr test.stderr
else
  echo "Stderr outputs match!"
fi

rm *.fs
rm test-file-*
rm *.std*