CC = gcc
CFLAGS = -g -Werror -Wall -Wno-enum-conversion -Wno-void-pointer-to-enum-cast 
programs: mcc mld

OBJDIR = build
MCC_SRCS = $(basename $(wildcard *.c))
MCC_OBJS = $(MCC_SRCS:%=%.o)

MLD_SRCS = $(basename $(wildcard ./linker/*.c))
MLD_OBJS = $(MLD_SRCS:%=%.o) util.o

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include

mcc: $(addprefix $(OBJDIR)/,$(MCC_OBJS))
	$(CC) $(CFLAGS) -o $@ $^

mld:$(addprefix $(OBJDIR)/,$(MLD_OBJS))
	$(CC) $(CFLAGS) -o $@ $^ -I ./include

recipes:
	rm -f parser.o
	rm -f ./mcc
	make mcc

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(OBJDIR)/linker/*.o
	rm -f ./mcc
	rm -f ./mld

test:
	$(info MCC SRCS="$(MCC_SRCS)")
	$(info MCC OBJS="$(MCC_OBJS)")
	$(info MLD SRCS="$(MLD_SRCS)")
	$(info MLD OBJS="$(MLD_OBJS)")
