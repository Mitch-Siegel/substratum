
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
                 bool printTac);

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

struct VariableEntry *scope_lookup_var_by_string(struct Scope *scope,
                                                 char *name);

struct VariableEntry *scope_lookup_var(struct Scope *scope,
                                       struct AST *name);

struct FunctionEntry *lookup_fun_by_string(struct Scope *scope,
                                           char *name);

struct FunctionEntry *scope_lookup_fun(struct Scope *scope,
                                 struct AST *name);

struct StructEntry *scope_lookup_struct(struct Scope *scope,
                                        struct AST *name);

struct StructEntry *scope_lookup_struct_by_type(struct Scope *scope,
                                                struct Type *type);

struct EnumEntry *scope_lookup_enum(struct Scope *scope,
                                    struct AST *name);

struct EnumEntry *scope_lookup_enum_by_type(struct Scope *scope,
                                            struct Type *type);

struct EnumEntry *scope_lookup_enum_by_member_name(struct Scope *scope,
                                                   char *name);

#endif
