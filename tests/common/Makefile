SBCC = ../../sbcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
RISCV_ARGS = -march=rv64g
sh=zsh

COMMON_DIR = ../common
SBCC_BUILD_DIR = ../../build
COVERAGE_BASE_DIR = ./coverage

TEST_NAME = $(notdir $(patsubst %/,%,$(CURDIR)))
programs: $(TEST_NAME).output

$(info "DEF: $(.DEFAULT_GOAL)")

%.S: %.sb
	$(eval COV_DEST_DIR = $(COVERAGE_BASE_DIR)/$(basename $@))
	mkdir -p $(COV_DEST_DIR)
	$(info "building file $@ from $<")
	$(SBCC) -i $^ -o $(basename $@).S -I $(COMMON_DIR) -I $(shell dirname $@)
	@mkdir -p $(COV_DEST_DIR)
	cp $(SBCC_BUILD_DIR)/*.gcda $(COV_DEST_DIR)
	geninfo --build-directory $(SBCC_BUILD_DIR) $(COV_DEST_DIR) -o $(COV_DEST_DIR)/$(basename $@).cov


# all test object files must have one and only one substratum source file
%.o: %.S
	$(AS) $(RISCV_ARGS) -r -o $@ $(basename $@).S

%.elf: $(shell find ./*.sb)
	$(eval SRCS := $(shell find . -type f -name '*.sb'))
	$(MAKE) $(SRCS:.sb=.S)
	$(MAKE) $(SRCS:.sb=.o)
	$(LD) -T ../link.lds -o $@ $(SRCS:.sb=.o)

%.output: %.txt %.elf
	$(eval ENTRY_POINT=$(shell riscv64-unknown-elf-readelf --file-header $(word 2,$^) | grep "Entry point address" | grep -Eo "0x[0-9abcdef]+"))
	$(eval $(shell qemu-system-riscv64 -machine virt -nographic -bios $(word 2,$^) -device loader,addr=$(ENTRY_POINT),cpu-num=0>$@))
	if [ `diff $@ $<` ]; then \
		echo "Test failed";\
		echo "output:";\
		echo $@;\
		exit 1;\
		else\
		echo "Test passed";\
	fi

clean:
	rm -f *.S
	rm -f *.o
	rm -f *.elf
	rm -f *.output
	rm -rf coverage


