#include <stdlib.h>
#include "util.h"
#include "symtab.h"
#include "tac.h"

#define MACHINE_REGISTER_COUNT 16
#define REGISTERS_TO_ALLOCATE 3
#define SCRATCH_REGISTER 0
#define SECOND_SCRATCH_REGISTER 1
#define RETURN_REGISTER 13

struct Lifetime
{
	int start, end, nwrites, nreads;
	char *name;
	enum variableTypes type;
	int indirectionLevel;
	int stackOrRegLocation;
	char isSpilled, isArgument;
	struct StackObjectEntry *localPointerTo;
};

struct Lifetime *newLifetime(char *name, enum variableTypes type, int indirectionLevel, int start);

char compareLifetimes(struct Lifetime *a, char *variable);

// update the lifetime start/end indices
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct LinkedList *ltList,
										char *name, 
										enum variableTypes type,
										int indirectionLevel,
										int newEnd);

struct Lifetime *updateOrInstertLifetimeFromTAC(struct LinkedList *ltList, struct TACOperand *operand, int index);

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct LinkedList *ltList,
						 struct TACOperand *writtenOperand,
						 int newEnd);

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
						struct TACOperand *readOperand,
						int newEnd);

struct LinkedList *findLifetimes(struct FunctionEntry *function);

int calculateRegisterLoading(struct LinkedList *activeLifetimes, int index);

// things more related to codegen than specifically register allocation

struct CodegenMetadata
{
	struct FunctionEntry *function; // symbol table entry for the function the register allocation data is for

	struct LinkedList *allLifetimes; // every lifetime that exists within this function

	// array allocated (of size largestTacIndex) for liveness analysis
	// index i contains a linkedList of all lifetimes active at TAC index i
	struct LinkedList **lifetimeOverlaps;

	// tracking for specialized lifetimes which may be removed from lifetimeOverlaps and need to be explicitly tracked
	struct Stack *spilledLifetimes;
	struct Stack *localPointerLifetimes;

	// largest TAC index for any basic block within the function
	int largestTacIndex;

	// flag 2 registers which should be used as scratch in case we have spilled variables (not always used)
	int reservedRegisters[2];

	// flag registers which have *ever* been used so we know what to callee-save
	char touchedRegisters[REGISTERS_TO_ALLOCATE];
};

// populate a linkedlist array so that the list at index i contains all lifetimes active at TAC index i
// then determine which variables should be spilled
int generateLifetimeOverlaps(struct CodegenMetadata *metadata);

// return the heuristic for how good a given lifetime is to spill - higher is better
int lifetimeHeuristic(struct Lifetime *lt);

void spillVariables(struct CodegenMetadata *metadata, int mostConcurrentLifetimes);

void sortSpilledLifetimes(struct CodegenMetadata *metadata);

// assign registers to variables which have registers
// assign spill addresses to variables which are spilled
void assignRegisters(struct CodegenMetadata *metadata);
