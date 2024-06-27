
#ifndef SYMTAB_SCOPE_H
#define SYMTAB_SCOPE_H

#include "ast.h"
#include "substratum_defs.h"
#include "type.h"
#include <stdio.h>

struct BasicBlock;
struct VariableEntry;

enum SCOPE_MEMBER_TYPE
{
    E_VARIABLE,
    E_FUNCTION,
    E_ARGUMENT,
    E_STRUCT,
    E_ENUM,
    E_SCOPE,
    E_BASICBLOCK,
};

enum ACCESS
{
    A_PUBLIC,
    A_PRIVATE,
};

struct ScopeMember
{
    char *name;
    void *entry;
    enum SCOPE_MEMBER_TYPE type;
    enum ACCESS accessibility;
};

struct Scope
{
    struct Scope *parentScope;
    struct FunctionEntry *parentFunction;
    struct StructEntry *parentImpl;
    struct Stack *entries;
    u8 subScopeCount;
    char *name; // duplicate pointer from ScopeMember for ease of use
};

// scope functions
struct Scope *scope_new(struct Scope *parentScope,
                        char *name,
                        struct FunctionEntry *parentFunction,
                        struct StructEntry *parentImpl);

void scope_free(struct Scope *scope);

void scope_print(struct Scope *scope,
                 FILE *outFile,
                 size_t depth,
                 char printTAC);

void scope_insert(struct Scope *scope,
                  char *name,
                  void *newEntry,
                  enum SCOPE_MEMBER_TYPE type,
                  enum ACCESS accessibility);

struct Scope *scope_create_sub_scope(struct Scope *scope);

// given a scope, a type, and a current integer byte offset
/// compute and return how many bytes of padding is necessary to create the first offset at which the type would be aligned if stored
size_t scope_compute_padding_for_alignment(struct Scope *scope, struct Type *alignedType, size_t currentOffset);

// scope lookup functions
char scope_contains(struct Scope *scope,
                    char *name);

struct ScopeMember *scope_lookup(struct Scope *scope,
                                 char *name);

// adds an entry in the given scope denoting that the block is from that scope
void scope_add_basic_block(struct Scope *scope,
                         struct BasicBlock *block);

#endif
