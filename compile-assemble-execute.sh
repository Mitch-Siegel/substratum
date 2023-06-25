echo "building compiler"
cd compiler
if ! make; then
    exit
fi

echo "\ncompiling files"
# xargs -I {} sh -c "echo {}; ls -la {}"
ls testsrc/*.m | cut -d '.' -f1 | xargs -I {} sh -c "echo {}; ./mcc {}.m {}.o"
# if ! ./mcc testsrc ../assembler/main.asm; then
    # exit
# fi

echo "linking files"
cd testsrc
rm -f linked.o
touch linked.o
# xargs -I {} sh -c "echo {}; ls -la {}"
ls *.o | xargs -I {} sh -c "if ! ../mld -i linked.o -i {} -o linked.o; then exit; fi"

cd ..
./mld -i ./testsrc/linked.o -o ../assembler/linked.asm -e


cd ../assembler
if ! customasm ./linked.asm; then
    exit
fi

cd ../emu
if ! make; then
    exit
fi

time ./emu ../assembler/main.bin

