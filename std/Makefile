SBROOT = ..
SBCC = $(SBROOT)/sbcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
RISCV_ARGS = -march=rv64g
sh=zsh

STD_DIR = $(SBROOT)/std
SBCC_BUILD_DIR = ../build
COVERAGE_BASE_DIR = ./coverage

# find all subdirectories within STD_DIR, and add them to the the include path with -I for each directory
STD_SUBDIRS = $(shell find $(STD_DIR) -type d)
STD_SUBDIRS := $(filter-out $(STD_DIR)/control-flows,$(STD_SUBDIRS))
STD_INCLUDES = $(foreach dir,$(STD_SUBDIRS),-I $(dir))

SRCS = $(shell find . -type f -name "*.sb")
ASMS = $(patsubst %.sb,%.S,$(SRCS))
OBJS = $(patsubst %.sb,%.o,$(SRCS))

programs: stdsb.o

stdsb.o: $(OBJS)
	$(LD) -T $(SBROOT)/assembler/link.lds -o $@ $^

# don't automatically remove .elf files
.SECONDARY: *.elf *.S *.o

%.S: %.sb
	@echo "Building $(basename $@).S"
ifndef NO-COVERAGE
	rm -f $(SBCC_BUILD_DIR)/*.gcda
endif
	$(SBCC) $(STD_INCLUDES) -i $^ -o $(basename $@).S -I $(COMMON_DIR) -I $(shell dirname $@)
ifdef NO-COVERAGE
		@echo "Coverage disabled"
else
		$(eval COV_DEST_DIR = $(COVERAGE_BASE_DIR)/$(basename $@))
		@mkdir -p $(COV_DEST_DIR)
		cp $(SBCC_BUILD_DIR)/*.gcda $(COV_DEST_DIR)
	  @geninfo --build-directory $(SBCC_BUILD_DIR) $(COV_DEST_DIR) -o $(COV_DEST_DIR)/$(basename $@).cov
endif


# all test object files must have one and only one substratum source file
%.o: %.S
	@echo "Building $@"
	$(AS) $(RISCV_ARGS) --gdwarf-5 -r -o $@ $(basename $@).S

%.elf: $(OBJS)
	$(LD) -T ../link.lds -o $@ $^

info:
	$(info "SRCS = $(SRCS)")
	$(info "ASMS = $(ASMS)")
	$(info "OBJS = $(OBJS)")
	$(info "STD_SUBDIRS = $(STD_SUBDIRS)")
	$(info "STD_INCLUDES = $(STD_INCLUDES)")

clean:
	@rm -rf *.S
	@rm -rf *.o
	@rm -f *.elf
	@rm -f *.output
	@rm -rf coverage

