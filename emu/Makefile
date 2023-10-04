CC = g++ 
CFLAGS = -Werror -Wall -O2 -g --std=c++17 -Wno-deprecated-declarations
LDFLAGS = -lncurses
programs: emu

OBJDIR = ./build
EMU_SRCS = $(basename $(wildcard *.cpp))
EMU_OBJS = $(EMU_SRCS:%=%.o)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $< -I ./include

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -Wno-int-to-pointer-cast -o $@ $< -I ./include

emu: $(addprefix $(OBJDIR)/,$(EMU_OBJS))
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OBJDIR)/*.o
	rm -f ./emu