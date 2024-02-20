
SBCC = ../sbcc
AS = riscv64-unknown-elf-as

COMMON_DIR = ./common
COVERAGE_DIR = ./coverage
COMMON_SRCS = $(basename $(wildcard $(COMMON_DIR)/*.sb))
COMMON_OBJS = $(COMMON_SRCS:%=%.o)
SBCC_TESTS_DIRS = $(wildcard ./*/)
SBCC_TESTS = $(filter-out $(COMMON_DIR) $(COVERAGE_DIR), $(SBCC_TESTS_DIRS:/=))

ifndef VERBOSE
.SILENT:
endif

tests: $(COMMON_OBJS)
	@for test in $(SBCC_TESTS) ; do \
		ls $${test}/*.sb | xargs -I {} echo {} | cut -d'.' -f2 | xargs -I {} make .{}.o;\
		riscv64-unknown-elf-ld -T ./link.lds -o $${test}/$${test}.elf `ls ./common/*.o` `ls $${test}/*.o`;\
		ENTRY_POINT=`readelf --file-header $${test}/$${test}.elf | grep "Entry point address" | grep -Eo "0x[0-9abcdef]+"`;\
		VM_OUTPUT=`qemu-system-riscv64 -machine virt -nographic -bios $${test}/$${test}.elf -device loader,addr=$$ENTRY_POINT,cpu-num=0`;\
		diff <(echo "$$VM_OUTPUT") <(cat "$${test}/$${test}.txt");\
		DIFF_RESULT=$$?;\
		if [ $$DIFF_RESULT -eq 0 ]; then \
			echo "Test $${test} passed"; else \
			echo "Test $${test} failed";\
			echo "output:";\
			echo "$$VM_OUTPUT";\
			exit 1;\
		fi ;\
	done

coverage-report:
	$(info "TRACEFILES: $(addprefix -a ,$(shell find $(COVERAGE_DIR) -type f -name "*.cov"))")
	lcov $(addprefix -a ,$(shell find $(COVERAGE_DIR) -type f)) -o $(COVERAGE_DIR)/total-coverage.info
	genhtml $(COVERAGE_DIR)/total-coverage.info -o $(COVERAGE_DIR)/totalcoverage

# all test object files must have one and only one substratum source file
%.o: %.sb
	rm -f ../build/*.gcda
	$(info "building file $@ from $<")
	$(SBCC) -i $^ -o $(basename $@).S -I $(COMMON_DIR) -I $(shell dirname $@)
	@mkdir -p $(shell dirname $(COVERAGE_DIR)/$@)
	@geninfo "../build" -b ".." -o $(COVERAGE_DIR)/$(basename $@).cov
	$(AS) -r -o $@ $(basename $@).S

info:
	$(info "COMMON_SRCS: $(COMMON_SRCS)")
	$(info "COMMON_OBJS: $(COMMON_OBJS)")
	$(info "SBCC_TESTS_DIRS: $(SBCC_TESTS_DIRS)")
	$(info "SBCC_TESTS: $(SBCC_TESTS)")

clean:
	@find . -name "*.elf" | xargs rm -f
	@find . -name "*.S" | xargs rm -f
	@find . -name "*.o" | xargs rm -f
	@rm -rf coverage