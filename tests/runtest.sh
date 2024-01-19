#!/bin/sh

ENTRY_POINT=`readelf --file-header $1/$1.elf | grep "Entry point address" | grep -Eo "0x[0-9abcdef]+"`
#qemu-system-riscv64 -S -s -machine virt -nographic -bios $1/$1.elf -device loader,addr=$ENTRY_POINT,cpu-num=0
VM_OUTPUT=`qemu-system-riscv64 -machine virt -nographic -bios $1/$1.elf -device loader,addr=$ENTRY_POINT,cpu-num=0`

echo "output:"
echo "$VM_OUTPUT"
echo "result:"

diff <(echo "$VM_OUTPUT") <(cat "$1/$1.txt")

DIFF_RESULT=$?
if [ $DIFF_RESULT -eq 0 ]
then
    echo "Test '$1' passed"
else
    echo "Test '$1' failed"
fi