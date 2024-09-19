#!/bin/sh

CWD=$(pwd)

# ensure the working directory is ends in substratum/std
if [ $(basename $CWD) != "std" ]; then
    echo "Error: This script must be run from the std directory"
    exit 1
fi

DIRS=". containers/string mem"
INCLUDE_PATHS=""
for DIR in $DIRS; do
    INCLUDE_PATHS="$INCLUDE_PATHS -I $CWD/$DIR"
done

FILES="mem/alloc.sb assert.sb option.sbh"
COMPILE_FILES=""
for FILE in $FILES; do
    COMPILE_FILES="$COMPILE_FILES -i $CWD/$FILE"
done

echo "Building stdlib.S from $COMPILE_FILES"
echo "compile command: ../sbcc $COMPILE_FILES -o $CWD/stdlib.S $INCLUDE_PATHS"
# execute the compiler
../sbcc $COMPILE_FILES -o $CWD/stdlib.S $INCLUDE_PATHS
if [ $? -ne 0 ]; then
    echo "Error: Failed to build stdlib.S"
    exit 1
else
    echo "Success: stdlib.S built"
fi
