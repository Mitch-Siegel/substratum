set -e
echo "building compiler"
cd compiler
if ! make; then
    exit
fi

echo "\ncompiling files"
# xargs -I {} sh -c "echo {}; ls -la {}"
ls testsrc/*.m | cut -d '.' -f1 | xargs -I {} sh -c "echo {};./cacc {}.ca {}.o || exit 255"
# if ! ./mcc testsrc ../assembler/main.asm; then
    # exit
# fi

echo "linking files"
cd testsrc
rm -f linked.o
touch linked.o
# xargs -I {} sh -c "echo {}; ls -la {}"
ls *.o | xargs -I {} sh -c "../cald -i linked.o -i {} -o linked.o || exit 255"

cd ..
./cald -i ./testsrc/linked.o -o ../assembler/linked.asm -e


cd ../assembler
if ! customasm ./linked.asm; then
    exit
fi

cd ../emu
if ! make; then
    exit
fi

time ./emu ../assembler/linked.bin

