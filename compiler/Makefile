CC = gcc
CFLAGS = -g -Werror -Wall -Wno-enum-conversion -Wno-void-pointer-to-enum-cast -fsanitize=address
programs: mcc

OBJDIR = build
MCC_SRCS = $(basename $(wildcard *.c))
MCC_OBJS = $(MCC_SRCS:%=%.o)

mcc: $(addprefix $(OBJDIR)/,$(MCC_OBJS))
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include

recipes:
	rm -f parser.o
	rm -f ./mcc
	make mcc

clean:
	rm -f $(OBJDIR)/*.o
	rm -f ./mcc

test:
	$(info SRCS="$(MCC_SRCS)")
	$(info OBJS="$(MCC_OBJS)")
