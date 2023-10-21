linked: linked.o link.lds
	riscv64-unknown-elf-ld -T link.lds -o linked.elf linked.o

linked.o: linked.s
	riscv64-unknown-elf-as -o linked.o linked.s

# use `CTRL+a` followed by `x` to exit
run: linked
	qemu-system-riscv64 -machine virt -nographic -kernel linked.elf -bios none

# use -s to listen for gdb connections on port 1234
# 	-S to not start the guest until told to do so from gdb
run-for-debug: linked
	qemu-system-riscv64 -s -S -machine virt -nographic -kernel linked.elf -bios none

# to be run in a separate terminal, after `make run`
debug: linked
	riscv64-unknown-elf-gdb ./linked.elf -x debug.gdb



clean:
	rm -f linked linked.o