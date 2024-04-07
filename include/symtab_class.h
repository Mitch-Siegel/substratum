#ifndef SYMTAB_CLASS_H
#define SYMTAB_CLASS_H

#include "symtab_variable.h"
#include "ast.h"

struct ClassMemberOffset
{
    struct VariableEntry *variable;
    int offset;
};

struct ClassEntry
{
    char *name;
    struct Scope *members;
    struct Stack *memberLocations;
    int totalSize;
};

// this represents the definition of a class itself, instantiation falls under variableEntry
struct ClassEntry *Scope_createClass(struct Scope *scope,
                                     char *name);

// given a VariableEntry corresponding to a class member which was just declared
// generate a ClassMemberOffset with the aligned location of the member within the class
void Class_assignOffsetToMemberVariable(struct ClassEntry *class,
                                        struct VariableEntry *v);

struct ClassMemberOffset *Class_lookupMemberVariable(struct ClassEntry *class,
                                                     struct AST *name);

struct ClassEntry *Scope_lookupClass(struct Scope *scope,
                                     struct AST *name);

struct ClassEntry *Scope_lookupClassByType(struct Scope *scope,
                                           struct Type *type);

#endif
