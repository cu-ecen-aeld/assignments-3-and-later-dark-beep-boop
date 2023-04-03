#!/bin/sh

if [ $# -lt 2 ]; then
	printf 'Usage: %s <writefile> <writestr>\n' $0
	exit 1
fi

writefile=$1
writestr=$2

dir=`dirname $writefile`

# Check if the directory exists and create it if necessary
test -d "$dir"
dir_error=$?
if [ $dir_error -eq 1 ]; then
	mkdir -p $dir
	dir_error=$?
fi

if [ $dir_error -ne 0 ]; then
	printf "Directory %s couldn't be created\n" $dir
	exit 1
fi

# Create or overwrite the file if the directory exists
[ $dir_error -eq 0 ] && echo "$writestr" > $writefile
write_error=$?

if [ $write_error -ne 0 ]; then
	printf "File %s couldn't be written\n" $writefile
	exit 1
fi

