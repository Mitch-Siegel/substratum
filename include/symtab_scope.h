
#ifndef SYMTAB_SCOPE_H
#define SYMTAB_SCOPE_H

#include "ast.h"
#include "substratum_defs.h"
#include "type.h"
#include <stdio.h>

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

enum Access
{
    a_public,
    a_private,
};

struct ScopeMember
{
    char *name;
    void *entry;
    enum ScopeMemberType type;
    enum Access accessibility;
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
                  enum ScopeMemberType type,
                  enum Access accessibility);

struct Scope *Scope_createSubScope(struct Scope *scope);

// given a scope, a type, and a current integer byte offset
/// compute and return how many bytes of padding is necessary to create the first offset at which the type would be aligned if stored
size_t Scope_ComputePaddingForAlignment(struct Scope *scope, struct Type *alignedType, size_t currentOffset);

// scope lookup functions
char Scope_contains(struct Scope *scope,
                    char *name);

struct ScopeMember *Scope_lookup(struct Scope *scope,
                                 char *name);

// adds an entry in the given scope denoting that the block is from that scope
void Scope_addBasicBlock(struct Scope *scope,
                         struct BasicBlock *block);

#endif
