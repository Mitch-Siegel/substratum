loaderstub.o: loaderstub.S
	riscv64-unknown-elf-as -r -o $@ $<

linked.elf: program.o link.lds
	riscv64-unknown-elf-ld -T link.lds -o linked.elf loaderstub.o program.o

# use `CTRL+a` followed by `x` to exit
run: linked.elf
	qemu-system-riscv64 -machine virt -nographic -kernel linked.elf -bios none

# use -s to listen for gdb connections on port 1234
# 	-S to not start the guest until told to do so from gdb
run-for-debug: linked.elf
	qemu-system-riscv64 -s -S -machine virt -nographic -kernel linked.elf -bios none

# to be run in a separate terminal, after `make run`
debug:
	riscv64-unknown-elf-gdb ./linked.elf -x debug.gdb



clean:
	rm -f linked linked.o