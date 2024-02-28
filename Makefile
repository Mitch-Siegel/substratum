CC = gcc
CFLAGS = -g -Werror -Wall -Wno-enum-conversion -Wno-void-pointer-to-enum-cast -Wno-deprecated-declarations -Wno-unknown-warning-option -fsanitize=address
programs: sbcc

ifdef COVERAGE
$(info "building sbcc with coverage/profiling enabled")
CFLAGS += --coverage
endif


OBJDIR = build
SBCC_SRCS = $(basename $(wildcard *.c))
SBCC_OBJS = $(SBCC_SRCS:%=%.o)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include

$(OBJDIR)/parser.o : parser.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include -Wno-discarded-qualifiers -Wno-incompatible-pointer-types-discards-qualifiers -Wno-unused-but-set-variable

sbcc: $(addprefix $(OBJDIR)/,$(SBCC_OBJS)) $(OBJDIR)/parser.o
	$(CC) $(CFLAGS) -o $@ $^

parser: parser.peg
	packcc -a parser.peg
	mv parser.h ./include

clean:
	rm -f $(OBJDIR)/*
	rm -f ./sbcc

info:
	$(info SBCC SRCS="$(SBCC_SRCS)")
	$(info SBCC OBJS="$(SBCC_OBJS)")
