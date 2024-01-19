CC = gcc
CFLAGS = -g -Werror -Wall -Wno-enum-conversion -Wno-void-pointer-to-enum-cast -Wno-deprecated-declarations -fsanitize=address
programs: sbcc

OBJDIR = build
SBCC_SRCS = $(basename $(wildcard *.c))
SBCC_OBJS = $(SBCC_SRCS:%=%.o)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include

$(OBJDIR)/parser.o : parser.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include -Wno-discarded-qualifiers

sbcc: $(addprefix $(OBJDIR)/,$(SBCC_OBJS)) $(OBJDIR)/parser.o
	$(CC) $(CFLAGS) -o $@ $^

parser: parser.peg
	packcc -a parser.peg
	mv parser.h ./include

recipes:
	rm -f parser.o
	rm -f ./sbcc
	make sbcc

clean:
	rm -f $(OBJDIR)/*.o
	rm -f ./sbcc

info:
	$(info SBCC SRCS="$(SBCC_SRCS)")
	$(info SBCC OBJS="$(SBCC_OBJS)")
