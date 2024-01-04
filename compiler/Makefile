CC = gcc
CFLAGS = -g -Werror -Wall -Wno-enum-conversion -Wno-void-pointer-to-enum-cast -Wno-deprecated-declarations -fsanitize=address
programs: parser cacc cald capp 

OBJDIR = build
CACC_SRCS = $(basename $(wildcard *.c))
CACC_OBJS = $(CACC_SRCS:%=%.o)

CALD_SRCS = $(basename $(wildcard ./linker/*.c))
CALD_OBJS = $(CALD_SRCS:%=%.o) util.o

CAPP_SRCS = $(basename $(wildcard ./preprocessor/*.c))
CAPP_OBJS = $(CAPP_SRCS:%=%.o) util.o

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include

$(OBJDIR)/parser.o : parser.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include -Wno-discarded-qualifiers

cacc: $(addprefix $(OBJDIR)/,$(CACC_OBJS)) $(OBJDIR)/parser.o
	$(CC) $(CFLAGS) -o $@ $^

cald:$(addprefix $(OBJDIR)/,$(CALD_OBJS))
	$(CC) $(CFLAGS) -o $@ $^ -I ./include

capp:$(addprefix $(OBJDIR)/,$(CAPP_OBJS))
	$(CC) $(CFLAGS) -o $@ $^ -I ./include

parser: parser.peg
	packcc -a parser.peg
	mv parser.h ./include

recipes:
	rm -f parser.o
	rm -f ./cacc
	make cacc

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(OBJDIR)/linker/*.o
	rm -f $(OBJDIR)/preprocessor/*.o
	rm -f ./cacc
	rm -f ./cald
	rm -f ./capp

test:
	$(info CACC SRCS="$(CACC_SRCS)")
	$(info CACC OBJS="$(CACC_OBJS)")
	$(info CALD SRCS="$(CALD_SRCS)")
	$(info CALD OBJS="$(CALD_OBJS)")
