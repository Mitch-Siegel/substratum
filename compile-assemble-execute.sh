cd compiler && make clean
if ! make; then
    exit
fi

if ! ./mcc testsrc ../assembler/main.asm; then
    exit
fi

cd ../assembler
if ! customasm ./main.asm; then
    exit
fi

cd ../emu
if ! make; then
    exit
fi

time ./emu ../assembler/main.bin

