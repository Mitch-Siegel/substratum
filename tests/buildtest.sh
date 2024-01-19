#!/bin/sh
set -e
set -x # echo on

if [ ! $1 ]
then
    echo "please provide an argument for which directory to build in!"
    exit 1
fi

cd $1

echo ""
echo "compiling files..."
# xargs -I {} sh -c "echo {}; ls -la {}"
rm -f build/*
ls *.ca | cut -d '.' -f1 | xargs -I {} sh -c "../../cacc -i {}.ca -o {}.S -I include && riscv64-unknown-elf-as -r -o {}.o {}.S || exit 255"

echo "linking files"
OBJ_FILES=$(ls *.o)
echo "Object files to link: $OBJ_FILES"
riscv64-unknown-elf-ld -T ../link.lds -o $1.elf $OBJ_FILES

