SBROOT = ../../..
SBCC = $(SBROOT)/sbcc
AS = riscv64-unknown-linux-gnu-as
LD = riscv64-unknown-linux-gnu-ld
LD_ARGS = -static --sysroot=/opt/riscv/sysroot 
LD_ARGS += --allow-multiple-definition -m elf64lriscv
LD_LIBC_STUFF = -L/opt/riscv/sysroot/usr/lib /opt/riscv/sysroot/usr/lib/libc.a
LD_LIBC_STUFF += -L/opt/riscv/lib/gcc/riscv64-unknown-linux-gnu/13.2.0/ /opt/riscv/lib/gcc/riscv64-unknown-linux-gnu/13.2.0/libgcc.a
LD_LIBC_STUFF += /opt/riscv/lib/gcc/riscv64-unknown-linux-gnu/13.2.0/libgcc_eh.a
LD_CRT_FILES = /opt/riscv/sysroot/usr/lib/crt1.o
RISCV_ARGS = -march=rv64g 
sh=zsh

STD_DIR = $(SBROOT)/std
SBCC_BUILD_DIR = ../build
COVERAGE_BASE_DIR = ./coverage

# find all subdirectories within STD_DIR, and add them to the the include path with -I for each directory
STD_SUBDIRS = $(shell find $(STD_DIR) -type d)
STD_SUBDIRS := $(filter-out $(STD_DIR)/control-flows,$(STD_SUBDIRS))
STD_INCLUDES = $(foreach dir,$(STD_SUBDIRS),-I $(dir))

SRCS = $(wildcard ./*.sb)
ASMS = $(patsubst %.sb,%.S,$(SRCS))
OBJS = $(patsubst %.sb,%.o,$(SRCS)) $(SBROOT)/std/stdsb.a

programs: fileio

.default: fileio

.PHONY: run debug

$(SBROOT)/std/stdsb.o:
	make -C $(SBROOT)/std

# don't automatically remove .elf files
.SECONDARY: *.elf *.S


fileio: $(OBJS)
	$(LD) $(LD_ARGS) -o $@ $^ $(LD_CRT_FILES) $(LD_LIBC_STUFF)

%.S: %.sb
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
	$(AS) $(RISCV_ARGS) --gdwarf-5 -r -o $@ $(basename $@).S

run: fileio
	$(eval $(shell qemu-riscv64 $(word 1,$^) >$@))

debug: fileio
	qemu-riscv64 -g 1234 $(word 1,$^)

info:
	$(info "SRCS = $(SRCS)")
	$(info "ASMS = $(ASMS)")
	$(info "OBJS = $(OBJS)")
	$(info "STD_SUBDIRS = $(STD_SUBDIRS)")
	$(info "STD_INCLUDES = $(STD_INCLUDES)")

clean:
	@rm -f *.S
	@rm -f *.o
	@rm -f *.elf
	@rm -f *.output
	@rm -rf coverage

