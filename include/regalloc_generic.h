#ifndef REGALLOC_GENERIC_H
#define REGALLOC_GENERIC_H

#include <stdio.h>
#include <stdlib.h>

#include "substratum_defs.h"
#include "type.h"

struct TACOperand;
struct TACLine;
struct LinkedList;
struct Set;
struct Scope;

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
    union
    {
        ssize_t stackOffset;
        struct Register *regLocation;
    } writebackInfo;
    u8 isArgument;
};

struct MachineInfo *MachineInfo_New(u8 maxReg,
                                    u8 n_temps,
                                    u8 n_arguments,
                                    u8 n_no_save,
                                    u8 n_callee_save,
                                    u8 n_caller_save);

void MachineInfo_Free(struct MachineInfo *info);

struct Lifetime *Lifetime_New(char *name,
                              struct Type *type,
                              size_t start,
                              u8 isGlobal,
                              u8 mustSpill);

size_t Lifetime_Hash(struct Lifetime *lifetime);

ssize_t Lifetime_Compare(struct Lifetime *lifetimeA, struct Lifetime *lifetimeB);

bool Lifetime_IsLiveAtIndex(struct Lifetime *lifetime, size_t index);

// update the lifetime start/end indices
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct Set *ltList,
                                        char *name,
                                        struct Type *type,
                                        size_t newEnd,
                                        u8 isGlobal,
                                        u8 mustSpill);

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct Set *ltList,
                         struct TACOperand *writtenOperand,
                         struct Scope *scope,
                         size_t newEnd);

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct Set *ltList,
                        struct TACOperand *readOperand,
                        struct Scope *scope,
                        size_t newEnd);

struct Set *findLifetimes(struct Scope *scope, struct LinkedList *basicBlockList);

struct Register
{
    struct Lifetime *containedLifetime; // lifetime contained within this register
    u8 index;                           // numerical index of this register
    char *name;                         // string name of this register (as it appears in asm)
};

struct Register *Register_New(u8 index);

bool Register_IsLive(struct Register *reg, size_t index);

struct MachineInfo
{
    // specifically required for register allocation - may be a subset of the entire machine register set
    struct Register *returnAddress;
    struct Register *stackPointer;
    struct Register *framePointer;
    struct Register *returnValue;
    struct Register **temps;
    u8 *tempsOccupied;
    struct Register **arguments;
    u8 n_arguments;
    u8 n_temps;

    // all registers (whether or not they fall into the above categories) must have a calling convention defined
    struct Register **no_save;
    struct Register **callee_save;
    struct Register **caller_save;
    u8 n_no_save;
    u8 n_callee_save;
    u8 n_caller_save;

    // basic info about the registers
    struct Register **allRegisters;
    u8 maxReg;
};

extern struct MachineInfo *(*setupMachineInfo)();

// things more related to codegen than specifically register allocation

struct RegallocMetadata
{
    struct FunctionEntry *function; // symbol table entry for the function the register allocation data is for
    struct Scope *scope;            // scope at which we are generating (identical to function->mainScope if in a function)

    struct Set *allLifetimes; // every lifetime that exists within this function based on variables and TAC operands (puplated during regalloc)

    struct Set *touchedRegisters;

    // largest TAC index for any basic block within the function
    size_t largestTacIndex;

    ssize_t argStackSize;
    ssize_t localStackSize;
};

// populate a linkedlist array so that the list at index i contains all lifetimes active at TAC index i
// then determine which variables should be spilled
size_t generateLifetimeOverlaps(struct RegallocMetadata *metadata);

// assign registers to variables which have registers
// assign spill addresses to variables which are spilled
void assignRegisters(struct RegallocMetadata *metadata);

#endif
