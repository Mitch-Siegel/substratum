#ifndef SYMTAB_STRUCT_H
#define SYMTAB_STRUCT_H

#include "ast.h"
#include "symtab_variable.h"

struct StructMemberOffset
{
    struct VariableEntry *variable;
    ssize_t offset;
};

struct StructEntry
{
    char *name;
    struct Scope *members;
    struct Stack *memberLocations;
    size_t totalSize;
};

// this represents the definition of a struct itself, instantiation falls under variableEntry
struct StructEntry *createStruct(struct Scope *scope,
                               char *name);

void StructEntry_free(struct StructEntry *theStruct);

// given a VariableEntry corresponding to a struct member which was just declared
// generate a StructMemberOffset with the aligned location of the member within the struct
void assignOffsetToMemberVariable(struct StructEntry *memberOf,
                                  struct VariableEntry *variable);

struct StructMemberOffset *lookupMemberVariable(struct StructEntry *theStruct,
                                               struct AST *name,
                                               struct Scope *scope);

struct FunctionEntry *lookupMethod(struct StructEntry *theStruct,
                                   struct AST *name,
                                   struct Scope *scope);

// TODO: char *name vs AST *name (change to AST *nameTree?)
struct FunctionEntry *lookupMethodByString(struct StructEntry *theStruct,
                                           char *name);

struct StructEntry *lookupStruct(struct Scope *scope,
                               struct AST *name);

struct StructEntry *lookupStructByType(struct Scope *scope,
                                     struct Type *type);

#endif
