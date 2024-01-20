#!/bin/sh
set -e
set -x # echo on

if [ ! $1 ]
then
    echo "please provide an argument for which directory to build in!"
    exit 1
fi

cd .. && make && cd -

cd common
ls *.sb | cut -d '.' -f1 | xargs -I {} sh -c "../../sbcc -i {}.sb -o {}.S -I ../common && riscv64-unknown-elf-as -r -o {}.o {}.S || exit 255"
cd -

cd $1

echo ""
echo "compiling files..."
# xargs -I {} sh -c "echo {}; ls -la {}"
rm -f build/*
ls *.sb | cut -d '.' -f1 | xargs -I {} sh -c "../../sbcc -i {}.sb -o {}.S -I ../common && riscv64-unknown-elf-as -r -o {}.o {}.S || exit 255"

echo "linking files"
OBJ_FILES=$(ls *.o)
OBJ_FILES+=" "
OBJ_FILES+=$(ls ../common/*.o)
echo "Object files to link: $OBJ_FILES"
riscv64-unknown-elf-ld -T ../link.lds -o $1.elf $OBJ_FILES

