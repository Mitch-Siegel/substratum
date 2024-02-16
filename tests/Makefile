
SBCC = ../sbcc
AS = riscv64-unknown-elf-as

COMMON_DIR = ./common
COMMON_SRCS = $(basename $(wildcard $(COMMON_DIR)/*.sb))
COMMON_OBJS = $(COMMON_SRCS:%=%.o)
SBCC_TESTS_DIRS = $(wildcard ./*/)
SBCC_TESTS = $(filter-out $(COMMON_DIR), $(SBCC_TESTS_DIRS:/=))


tests: $(SBCC_TESTS)
	cd .. && make COVERAGE=true
	@for test in $^ ; do \
		echo $${test};\
	done

$(SBCC_TESTS):
	$(info "$@: $@ $^:$^")

%.o: %.sb
	cd .. && make COVERAGE=true
	$(call compile_test_object, $(basename $@))


define compile_test_object
	@echo "Compiling test object $1"
	$(SBCC) -i $1.sb -o $1.S -I ./common
	$(AS) -r -o $1.o $1.S
endef


info:
	$(info "COMMON_SRCS: $(COMMON_SRCS)")
	$(info "COMMON_OBJS: $(COMMON_OBJS)")
	$(info "SBCC_TESTS_DIRS: $(SBCC_TESTS_DIRS)")
	$(info "SBCC_TESTS: $(SBCC_TESTS)")

clean:
	@find . -name "*.elf" | xargs rm -f
	@find . -name "*.S" | xargs rm -f
	@find . -name "*.o" | xargs rm -f