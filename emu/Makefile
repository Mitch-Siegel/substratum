CC = g++ 
CFLAGS = -Werror -Wall -O2 -g --std=c++17
programs: emu

emu: emu.o memory.o core.o hardware.o names.o
	$(CC) $(CFLAGS) -o $@ $^

emu.o: emu.cpp
	$(CC) -c $(CFLAGS) emu.cpp 

hardware.o: hardware.cpp
	$(CC) -c $(CFLAGS) hardware.cpp 

core.o: core.cpp
	$(CC) -c $(CFLAGS) core.cpp 

memory.o: memory.cpp
	$(CC) -c $(CFLAGS) memory.cpp

names.o: names.cpp
	$(CC) -c $(CFLAGS) names.cpp

clean:
	rm *.o
	rm ./emu