CC = gcc
CFLAGS = -g -Werror -Wall -Wno-enum-conversion -Wno-void-pointer-to-enum-cast -Wno-deprecated-declarations -fsanitize=address
programs: cacc cald capp

OBJDIR = build
MCC_SRCS = $(basename $(wildcard *.c))
MCC_OBJS = $(MCC_SRCS:%=%.o)

MLD_SRCS = $(basename $(wildcard ./linker/*.c))
MLD_OBJS = $(MLD_SRCS:%=%.o) util.o

MPP_SRCS = $(basename $(wildcard ./preprocessor/*.c))
MPP_OBJS = $(MPP_SRCS:%=%.o) util.o

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include

cacc: $(addprefix $(OBJDIR)/,$(MCC_OBJS))
	$(CC) $(CFLAGS) -o $@ $^

cald:$(addprefix $(OBJDIR)/,$(MLD_OBJS))
	$(CC) $(CFLAGS) -o $@ $^ -I ./include

capp:$(addprefix $(OBJDIR)/,$(MPP_OBJS))
	$(CC) $(CFLAGS) -o $@ $^ -I ./include

recipes:
	rm -f parser.o
	rm -f ./mcc
	make mcc

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(OBJDIR)/linker/*.o
	rm -f $(OBJDIR)/preprocessor/*.o
	rm -f ./cacc
	rm -f ./cald
	rm -f ./capp

test:
	$(info MCC SRCS="$(MCC_SRCS)")
	$(info MCC OBJS="$(MCC_OBJS)")
	$(info MLD SRCS="$(MLD_SRCS)")
	$(info MLD OBJS="$(MLD_OBJS)")
