#include <stdlib.h>
#include "util.h"
#include "symtab.h"
#include "tac.h"

#ifndef _REGALLOC_H_
#define _REGALLOC_H_
#define MACHINE_REGISTER_COUNT 16
#define REGISTERS_TO_ALLOCATE 13
// definitions for what we intend to use as scratch registers when applicable
#define SCRATCH_REGISTER 0
#define SECOND_SCRATCH_REGISTER 1
// the return register is used as a de-facto n+1-th scratch register as many use cases cause infrequent need for an extra
// we can use this register without caring about stomping the value instead of reserving another
// we only need to preserve its value when we return something in it
#define RETURN_REGISTER 13

enum WritebackLocation
{
	wb_register,
	wb_stack,
	wb_global,
	wb_unknown,
};

struct Lifetime
{
	int start, end, nwrites, nreads;
	char *name;
	struct Type type;
	enum WritebackLocation wbLocation;
	int stackLocation;
	unsigned char registerLocation;
	char inRegister, onStack, isArgument;
};

struct Lifetime *newLifetime(char *name,
							 struct Type *type,
							 int start,
							 char isGlobal,
							 char mustSpill);

int compareLifetimes(struct Lifetime *a, char *variable);

// update the lifetime start/end indices
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct LinkedList *ltList,
										char *name,
										struct Type *type,
										int newEnd,
										char isGlobal,
										char mustSpill);

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct LinkedList *ltList,
						 struct TACOperand *writtenOperand,
						 struct Scope *scope,
						 int newEnd);

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
						struct TACOperand *readOperand,
						struct Scope *scope,
						int newEnd);

struct LinkedList *findLifetimes(struct Scope *scope, struct LinkedList *basicBlockList);

int calculateRegisterLoading(struct LinkedList *activeLifetimes, int index);

// things more related to codegen than specifically register allocation

struct CodegenMetadata
{
	struct FunctionEntry *function; // symbol table entry for the function the register allocation data is for

	struct LinkedList *allLifetimes; // every lifetime that exists within this function based on variables and TAC operands

	// array allocated (of size largestTacIndex) for liveness analysis
	// index i contains a linkedList of all lifetimes active at TAC index i
	struct LinkedList **lifetimeOverlaps;

	// tracking for lifetimes which live in registers
	struct LinkedList *registerLifetimes;

	// largest TAC index for any basic block within the function
	int largestTacIndex;

	// flag registers which should be used as scratch in case we have spilled variables (not always used, but can have up to 3)
	int reservedRegisterCount;
	int reservedRegisters[3];

	// flag registers which have *ever* been used so we know what to callee-save
	char touchedRegisters[MACHINE_REGISTER_COUNT];
};

// populate a linkedlist array so that the list at index i contains all lifetimes active at TAC index i
// then determine which variables should be spilled
int generateLifetimeOverlaps(struct CodegenMetadata *metadata);


// assign registers to variables which have registers
// assign spill addresses to variables which are spilled
void assignRegisters(struct CodegenMetadata *metadata);

/*
 * the main function for register allocation
 * finds lifetimes and lifetime overlaps
 * figures out which lifetimes are in contention for registers
 * then gives stack offset or register indices to all lifetimes
 * returns the number of bytes of stack space required for locals
 */

int allocateRegisters(struct CodegenMetadata *metadata, int optimizationLevel);

#endif
