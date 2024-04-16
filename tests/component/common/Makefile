SBCC = ../../../sbcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
RISCV_ARGS = -march=rv64g
sh=zsh

COMMON_DIR = ../common
SBCC_BUILD_DIR = ../../../build
COVERAGE_BASE_DIR = ./coverage

TEST_NAME = $(notdir $(patsubst %/,%,$(CURDIR)))

SRCS = $(wildcard ./*.sb)
ASMS = $(patsubst %.sb,%.S,$(SRCS))
OBJS = $(patsubst %.sb,%.o,$(SRCS))

# don't automatically remove .elf files
.SECONDARY: $(TEST_NAME).elf $(ASMS)

.DEFAULT_GOAL = $(TEST_NAME).output

.PHONY: always

always:

programs: $(TEST_NAME).output

%.S: %.sb
	rm -f $(SBCC_BUILD_DIR)/*.gcda
	$(eval COV_DEST_DIR = $(COVERAGE_BASE_DIR)/$(basename $@))
	mkdir -p $(COV_DEST_DIR)
	$(SBCC) -i $^ -o $(basename $@).S -I $(COMMON_DIR) -I $(shell dirname $@)
	@mkdir -p $(COV_DEST_DIR)
	cp $(SBCC_BUILD_DIR)/*.gcda $(COV_DEST_DIR)
	@geninfo --build-directory $(SBCC_BUILD_DIR) $(COV_DEST_DIR) -o $(COV_DEST_DIR)/$(basename $@).cov


# all test object files must have one and only one substratum source file
%.o: %.S
	$(AS) $(RISCV_ARGS) --gdwarf-5 -r -o $@ $(basename $@).S

%.elf: $(OBJS)
	$(LD) -T ../link.lds -o $@ $^

%.output: %.txt %.elf always
	$(eval ENTRY_POINT=$(shell riscv64-unknown-elf-readelf --file-header $(word 2,$^) | grep "Entry point address" | grep -Eo "0x[0-9abcdef]+"))
	$(eval $(shell qemu-system-riscv64 -machine virt -nographic -bios $(word 2,$^) -device loader,addr=$(ENTRY_POINT),cpu-num=0>$@))
	@DIFF_OUTPUT=$$(diff $@ $<); \
	if [ $$? -ne 0 ]; then \
		echo "Test '$(TEST_NAME)' failed"; \
		echo "output:"; \
		echo $$DIFF_OUTPUT; \
		exit 1; \
		else \
		echo "Test '$(TEST_NAME)' passed"; \
	fi

info:
	$(info "SRCS = $(SRCS)")
	$(info "ASMS = $(ASMS)")
	$(info "OBJS = $(OBJS)")

clean:
	@rm -f *.S
	@rm -f *.o
	@rm -f *.elf
	@rm -f *.output
	@rm -rf coverage


