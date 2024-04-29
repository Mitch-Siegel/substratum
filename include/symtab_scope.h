
#ifndef SYMTAB_SCOPE_H
#define SYMTAB_SCOPE_H

#include <stdio.h>
#include "ast.h"
#include "substratum_defs.h"
#include "type.h"

struct BasicBlock;
struct VariableEntry;

enum ScopeMemberType
{
    e_variable,
    e_function,
    e_argument,
    e_class,
    e_scope,
    e_basicblock,
};

struct ScopeMember
{
    char *name;
    enum ScopeMemberType type;
    void *entry;
};

struct Scope
{
    struct Scope *parentScope;
    struct FunctionEntry *parentFunction;
    struct Stack *entries;
    u8 subScopeCount;
    char *name; // duplicate pointer from ScopeMember for ease of use
};

// scope functions
struct Scope *Scope_new(struct Scope *parentScope,
                        char *name,
                        struct FunctionEntry *parentFunction);

void Scope_free(struct Scope *scope);

void Scope_print(struct Scope *scope,
                 FILE *outFile,
                 size_t depth,
                 char printTAC);

void Scope_insert(struct Scope *scope,
                  char *name,
                  void *newEntry,
                  enum ScopeMemberType type);

struct Scope *Scope_createSubScope(struct Scope *scope);

// given a scope, a type, and a current integer byte offset
/// compute and return how many bytes of padding is necessary to create the first offset at which the type would be aligned if stored
size_t Scope_ComputePaddingForAlignment(struct Scope *scope, struct Type *alignedType, size_t currentOffset);

// scope lookup functions
char Scope_contains(struct Scope *scope,
                    char *name);

struct ScopeMember *Scope_lookup(struct Scope *scope,
                                 char *name);

// gets the integer size (not aligned) of a given type
size_t getSizeOfType(struct Scope *scope, struct Type *type);

// gets the integer size (not aligned) of a given type, but based on the dereference level as (t->indirectionLevel - 1)
size_t getSizeOfDereferencedType(struct Scope *scope, struct Type *type);

size_t getSizeOfArrayElement(struct Scope *scope, struct VariableEntry *variable);

// calculate the power of 2 to which a given type needs to be aligned
u8 getAlignmentOfType(struct Scope *scope, struct Type *type);

// scope linearization functions

// adds an entry in the given scope denoting that the block is from that scope
void Scope_addBasicBlock(struct Scope *scope,
                         struct BasicBlock *block);

#endif
