#include <stdlib.h>
#include "util.h"
#include "symtab.h"
#include "tac.h"

#ifndef _REGALLOC_H_
#define _REGALLOC_H_
// definitions for what we intend to use as scratch registers when applicable
#define TEMP_0 5 // t0
#define TEMP_1 6 // t1
#define TEMP_2 7 // t2
#define RETURN_REGISTER 10
#define START_ALLOCATING_FROM a1
#define MACHINE_REGISTER_COUNT 32

enum riscvRegisters
{
	zero,
	ra,
	sp,
	gp,
	tp,
	t0,
	t1,
	t2,
	fp,
	s1,
	a0,
	a1,
	a2,
	a3,
	a4,
	a5,
	a6,
	a7,
	s2,
	s3,
	s4,
	s5,
	s6,
	s7,
	s8,
	s9,
	s10,
	s11,
	t3,
	t4,
	t5,
	t6,
};

enum WritebackLocation
{
	wb_register,
	wb_stack,
	wb_global,
	wb_unknown,
};

struct Lifetime
{
	size_t start, end, nwrites, nreads;
	char *name;
	struct Type type;
	enum WritebackLocation wbLocation;
	ssize_t stackLocation;
	unsigned char registerLocation;
	u8 inRegister, onStack, isArgument;
};

struct Lifetime *newLifetime(char *name,
							 struct Type *type,
							 size_t start,
							 u8 isGlobal,
							 u8 mustSpill);

int compareLifetimes(struct Lifetime *compared, char *variable);

// update the lifetime start/end indices
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct LinkedList *ltList,
										char *name,
										struct Type *type,
										size_t newEnd,
										u8 isGlobal,
										u8 mustSpill);

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct LinkedList *ltList,
						 struct TACOperand *writtenOperand,
						 struct Scope *scope,
						 size_t newEnd);

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
						struct TACOperand *readOperand,
						struct Scope *scope,
						size_t newEnd);

struct LinkedList *findLifetimes(struct Scope *scope, struct LinkedList *basicBlockList);

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
	size_t largestTacIndex;

	// flag registers which should be used as scratch in case we have spilled variables (not always used, but can have up to 3)
	u8 reservedRegisterCount;
	u8 reservedRegisters[3];

	// flag registers which have *ever* been used so we know what to callee-save
	char touchedRegisters[MACHINE_REGISTER_COUNT];

	u8 nRegistersCalleeSaved;

	size_t localStackSize;		 // number of bytes required to store store all local stack variables of a function (aligned to MACHINE_REGISTER_SIZE_BYTES because callee-saved registers are at the stack addresses directly below these)
	size_t calleeSaveStackSize; // number of bytes required to store all callee-saved registers (aligned to MACHINE_REGISTER_SIZE_BYTES by starting from localStackSize and only storing MACHINE_REGISTER_SIZE_BYTES at a time)

	size_t totalStackSize;	// total number of bytes the function decrements the stack pointer to store all locals and callee-saved registers (aligned to STACK_ALIGN_BYTES)
};

// populate a linkedlist array so that the list at index i contains all lifetimes active at TAC index i
// then determine which variables should be spilled
size_t generateLifetimeOverlaps(struct CodegenMetadata *metadata);

// assign registers to variables which have registers
// assign spill addresses to variables which are spilled
void assignRegisters(struct CodegenMetadata *metadata);

#endif
