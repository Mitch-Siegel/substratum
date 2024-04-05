CC = gcc
CFLAGS = -g -Werror -Wall -Wno-enum-conversion -Wno-void-pointer-to-enum-cast -Wno-deprecated-declarations -Wno-unknown-warning-option -fsanitize=address
programs: sbcc

ifdef COVERAGE
$(info "building sbcc with coverage/profiling enabled")
CFLAGS += --coverage
endif


OBJDIR = build
SBCC_SRCS = $(filter-out parser, $(basename $(wildcard *.c)))
SBCC_OBJS = $(SBCC_SRCS:%=%.o)
INCLUDE_DIR = ./include

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I $(INCLUDE_DIR)

$(OBJDIR)/parser.o : parser.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I $(INCLUDE_DIR) -Wno-discarded-qualifiers -Wno-incompatible-pointer-types-discards-qualifiers -Wno-unused-but-set-variable

sbcc: $(OBJDIR)/parser.o $(addprefix $(OBJDIR)/,$(SBCC_OBJS)) 
	$(CC) $(CFLAGS) -o $@ $^

parser.c: parser.peg
	packcc -a parser.peg
	mv parser.h $(INCLUDE_DIR)

lint:
	$(info "SOURCES: $(SBCC_SRCS)")
	for SOURCE_FILE in $(SBCC_SRCS); do \
		clang-tidy-17 $$SOURCE_FILE.c -- -I $(INCLUDE_DIR); \
	done

clean:
	rm -f $(OBJDIR)/*
	rm -f ./sbcc
	rm -f parser.c
	rm -f $(INCLUDE_DIR)/parser.h

info:
	$(info SBCC SRCS="$(SBCC_SRCS)")
	$(info SBCC OBJS="$(SBCC_OBJS)")
