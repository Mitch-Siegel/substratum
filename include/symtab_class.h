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

void ClassEntry_free(struct ClassEntry *class);

// given a VariableEntry corresponding to a class member which was just declared
// generate a ClassMemberOffset with the aligned location of the member within the class
void assignOffsetToMemberVariable(struct ClassEntry *class,
                                  struct VariableEntry *variable);

struct ClassMemberOffset *lookupMemberVariable(struct ClassEntry *class,
                                               struct AST *name);

struct FunctionEntry *lookupMethod(struct ClassEntry *class,
                                   struct AST *name);

// TODO: char *name vs AST *name (change to AST *nameTree?)
struct FunctionEntry *lookupMethodByString(struct ClassEntry *class,
                                           char *name);

struct ClassEntry *lookupClass(struct Scope *scope,
                               struct AST *name);

struct ClassEntry *lookupClassByType(struct Scope *scope,
                                     struct Type *type);

#endif
