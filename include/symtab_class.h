#ifndef SYMTAB_CLASS_H
#define SYMTAB_CLASS_H

#include "ast.h"
#include "symtab_variable.h"

struct ClassMemberOffset
{
    struct VariableEntry *variable;
    ssize_t offset;
};

struct ClassEntry
{
    char *name;
    struct Scope *members;
    struct Stack *memberLocations;
    size_t totalSize;
};

// this represents the definition of a class itself, instantiation falls under variableEntry
struct ClassEntry *createClass(struct Scope *scope,
                               char *name);

// given a VariableEntry corresponding to a class member which was just declared
// generate a ClassMemberOffset with the aligned location of the member within the class
void assignOffsetToMemberVariable(struct ClassEntry *class,
                                  struct VariableEntry *variable);

struct ClassMemberOffset *lookupMemberVariable(struct ClassEntry *class,
                                               struct AST *name);

struct ClassEntry *lookupClass(struct Scope *scope,
                               struct AST *name);

struct ClassEntry *lookupClassByType(struct Scope *scope,
                                     struct Type *type);

#endif
