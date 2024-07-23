#ifndef REGALLOC_GENERIC_H
#define REGALLOC_GENERIC_H

#include <stdio.h>
#include <stdlib.h>

#include "substratum_defs.h"
#include "type.h"

#include "mbcl/array.h"
#include "mbcl/list.h"
#include "mbcl/set.h"

struct TACOperand;
struct TACLine;
struct LinkedList;
struct Scope;

enum WRITEBACK_LOCATION
{
    WB_REGISTER,
    WB_STACK,
    WB_GLOBAL,
    WB_UNKNOWN,
};

struct Lifetime
{
    size_t start, end, nwrites, nreads;
    char *name;
    struct Type type;
    enum WRITEBACK_LOCATION wbLocation;
    union
    {
        ssize_t stackOffset;
        struct Register *regLocation;
    } writebackInfo;
    u8 isArgument;
};

void *lifetime_find(Set *allLifetimes, char *lifetimeName);

struct Lifetime *lifetime_new(char *name,
                              struct Type *type,
                              size_t start,
                              u8 isGlobal,
                              u8 mustSpill);

size_t lifetime_hash(struct Lifetime *lifetime);

ssize_t lifetime_compare(struct Lifetime *lifetimeA, struct Lifetime *lifetimeB);

bool lifetime_is_live_at_index(struct Lifetime *lifetime, size_t index);

// update the lifetime start/end indices
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *update_or_insert_lifetime(Set *lifetimes,
                                           char *name,
                                           struct Type *type,
                                           size_t newEnd,
                                           u8 isGlobal,
                                           u8 mustSpill);

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void record_variable_write(Set *lifetimes,
                           struct TACOperand *writtenOperand,
                           struct Scope *scope,
                           size_t newEnd);

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void record_variable_read(Set *lifetimes,
                          struct TACOperand *readOperand,
                          struct Scope *scope,
                          size_t newEnd);

Set *find_lifetimes(struct Scope *scope, List *basicBlockList);

struct Register
{
    struct Lifetime *containedLifetime; // lifetime contained within this register
    u8 index;                           // numerical index of this register
    char *name;                         // string name of this register (as it appears in asm)
};

struct Register *register_new(u8 index);

bool register_is_live(struct Register *reg, size_t index);

ssize_t register_compare(void *dataA, void *dataB);

struct MachineInfo
{
    // specifically required for register allocation - may be a subset of the entire machine register set
    struct Register *returnAddress;
    struct Register *stackPointer;
    struct Register *framePointer;
    struct Register *returnValue;
    Array temps;
    Array tempsOccupied;
    Array arguments;
    Array generalPurpose;

    // all registers (whether or not they fall into the above categories) must have a calling convention defined
    Array no_save;
    Array callee_save;
    Array caller_save;

    // basic info about the registers
    Array allRegisters;
};

extern struct MachineInfo *(*setupMachineInfo)();

struct MachineInfo *machine_info_new(u8 maxReg,
                                     u8 n_temps,
                                     u8 n_arguments,
                                     u8 n_general_purpose,
                                     u8 n_no_save,
                                     u8 n_callee_save,
                                     u8 n_caller_save);

void machine_info_free(struct MachineInfo *info);

struct Register *find_register_by_name(struct MachineInfo *info, char *name);

// things more related to codegen than specifically register allocation

struct RegallocMetadata
{
    struct FunctionEntry *function; // symbol table entry for the function the register allocation data is for
    struct Scope *scope;            // scope at which we are generating (identical to function->mainScope if in a function)

    Set *allLifetimes; // every lifetime that exists within this function based on variables and TAC operands (puplated during regalloc)

    Set *touchedRegisters;

    // largest TAC index for any basic block within the function
    size_t largestTacIndex;

    ssize_t argStackSize;
    ssize_t localStackSize;
};

#endif
