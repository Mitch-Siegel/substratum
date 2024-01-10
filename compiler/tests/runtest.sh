#!/bin/sh

ENTRY_POINT=`readelf --file-header $1/$1.elf | grep "Entry point address" | grep -Eo "0x[0-9abcdef]+"`
#qemu-system-riscv64 -S -s -machine virt -nographic -bios $1/$1.elf -device loader,addr=$ENTRY_POINT,cpu-num=0
qemu-system-riscv64 -machine virt -nographic -bios $1/$1.elf -device loader,addr=$ENTRY_POINT,cpu-num=0