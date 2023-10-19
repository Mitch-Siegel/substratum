linked: linked.o link.lds
	riscv64-unknown-elf-ld -T link.lds -o linked linked.o

linked.o: linked.s
	riscv64-unknown-elf-as -o linked.o linked.s

# use `CTRL+a` followed by `x` to exit
run: linked
	qemu-system-riscv32 -machine virt -nographic -kernel linked -bios none

clean:
	rm linked linked.o