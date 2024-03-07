SBCC = ../../sbcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
RISCV_ARGS = -march=rv64g

COMMON_DIR = ../common
GCOV_BASE = ./coverage

.DEFAULT_GOAL = $(notdir $(patsubst %/,%,$(CURDIR))).elf

%.S: %.sb
	$(eval COV_OUT_DIR = $(GCOV_BASE)/$(basename $@)/)
	mkdir -p $(COV_OUT_DIR)
	$(info "GCOV_DIR: $(COV_OUT_DIR)")
	$(info "building file $@ from $<")
	@export GCOV_PREFIX = $(COV_OUT_DIR) && $(SBCC) -i $^ -o $(basename $@).S -I $(COMMON_DIR) -I $(shell dirname $@)
	@mkdir -p $(shell dirname $(COVERAGE_DIR)/$@)
	@geninfo $(COV_OUT_DIR) -b ".." -o $(COV_OUT_DIR)/$(basename $@).cov


# all test object files must have one and only one substratum source file
%.o: %.S
	$(AS) $(RISCV_ARGS) -r -o $@ $(basename $@).S

%.elf: $(shell find ./*.sb)
	$(eval SRCS := $(shell find . -type f -name '*.sb'))
	$(MAKE) $(SRCS:.sb=.S)
	$(MAKE) $(SRCS:.sb=.o)
	$(LD) -T ../link.lds -o $@ $(SRCS:.sb=.o)

clean:
	rm -f *.S
	rm -f *.o
	rm -f *.elf
	rm -rf coverage


# @for test in $(SBCC_TESTS) ; do \
# 		ls $${test}/*.sb | xargs -I {} echo {} | cut -d'.' -f2 | xargs -I {} make .{}.o;\
# 		 -T ./link.lds -o $${test}/$${test}.elf `ls ./common/*.o` `ls $${test}/*.o`;\
# 		ENTRY_POINT=`readelf --file-header $${test}/$${test}.elf | grep "Entry point address" | grep -Eo "0x[0-9abcdef]+"`;\
# 		VM_OUTPUT=`qemu-system-riscv64 -machine virt -nographic -bios $${test}/$${test}.elf -device loader,addr=$$ENTRY_POINT,cpu-num=0`;\
# 		diff <(echo "$$VM_OUTPUT") <(cat "$${test}/$${test}.txt");\
# 		DIFF_RESULT=$$?;\
# 		if [ $$DIFF_RESULT -eq 0 ]; then \
# 			echo "Test $${test} passed"; else \
# 			echo "Test $${test} failed";\
# 			echo "output:";\
# 			echo "$$VM_OUTPUT";\
# 			exit 1;\
# 		fi ;\
# 	done


