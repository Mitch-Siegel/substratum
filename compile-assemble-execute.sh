set -e
set -x # echo on

if [ ! $1 ]
then
    echo "please provide an argument to be passed to the assembler makefile ('run' or 'run-for-debug')"
    exit 1
fi

echo "building compiler"
cd compiler
if ! make -j`nproc`; then
    exit
fi

echo ""
echo "compiling files..."
# xargs -I {} sh -c "echo {}; ls -la {}"
rm -f testsrc/*.o
ls testsrc/*.ca | cut -d '.' -f1 | xargs -I {} sh -c "./cacc -i {}.ca -o {}.S -I testsrc && riscv64-unknown-elf-as -r -o {}.o {}.S || exit 255"
# if ! ./mcc testsrc ../assembler/main.asm; then
    # exit
# fi

echo "linking files"
OBJ_FILES=$(ls testsrc/*.o)
echo "Object files to link: $OBJ_FILES"
riscv64-unknown-elf-ld -T ../assembler/link.lds -o ../assembler/linked.elf $OBJ_FILES


cd ../assembler
if ! make $1; then
    cd ..
    exit $?
fi

cd ..
