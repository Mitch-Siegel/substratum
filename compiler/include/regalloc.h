#include <stdlib.h>
#include "util.h"
#include "symtab.h"
#include "tac.h"

#define REGISTER_COUNT 4

struct Lifetime
{
	int start, end, nwrites, nreads;
	char *variable;
	int stackOrRegLocation;
	enum variableTypes type;
	char isSpilled, isArgument;
	struct StackObjectEntry *localPointerTo;
};

struct Lifetime *newLifetime(char *variable, enum variableTypes type, int start);

char compareLifetimes(struct Lifetime *a, char *variable);

// update the lifetime start/end indices
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct LinkedList *ltList,
										char *variable,
										enum variableTypes type,
										int newEnd);

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct LinkedList *ltList,
						 char *variable,
						 enum variableTypes type,
						 int newEnd);

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
						char *variable,
						enum variableTypes type,
						int newEnd);

// places a variable in a register, with no guarantee that it is modifiable
int placeOrFindOperandInRegister(struct LinkedList *lifetimes, char *variable, struct LinkedList *currentBlock, int registerIndex, char *touchedRegisters);

struct LinkedList *findLifetimes(struct FunctionEntry *function);


