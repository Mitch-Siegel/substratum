CC = gcc
CFLAGS = -g -Werror -Wall -Wno-enum-conversion -Wno-void-pointer-to-enum-cast
programs: mcc

mcc: compiler.o parser.o ast.o tac.o symtab.o util.o linearizer.o regalloc.o codegen.o serialize.o
	$(CC) $(CFLAGS) -o $@ $^

compiler.o: compiler.c
	$(CC) -c $(CFLAGS) compiler.c

parser.o: parser.c parser.h
	$(CC) -c $(CFLAGS) parser.c

ast.o: ast.c ast.h
	$(CC) -c $(CFLAGS) ast.c

tac.o: tac.c tac.h
	$(CC) -c $(CFLAGS) tac.c

symtab.o: symtab.c symtab.h
	$(CC) -c $(CFLAGS) symtab.c

util.o: util.c util.h
	$(CC) -c $(CFLAGS) util.c

linearizer.o: linearizer.c linearizer.h
	$(CC) -c $(CFLAGS) linearizer.c

regalloc.o: regalloc.c regalloc.h
	$(CC) -c $(CFLAGS) regalloc.c

codegen.o: codegen.c codegen.h
	$(CC) -c $(CFLAGS) codegen.c

serialize.o: serialize.c serialize.h
	$(CC) -c $(CFLAGS) serialize.c



clean:
	rm *.o
	rm ./mcc
