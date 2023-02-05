cd assembler && customasm ./main.asm
cd ../emu && make
sleep 1
./emu ../assembler/main.bin

