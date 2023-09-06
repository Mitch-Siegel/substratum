cd ./assembler
if ! customasm ./linked.asm; then
    exit
fi

cd ../emu
if ! make; then
    exit
fi

time ./emu ../assembler/linked.bin
