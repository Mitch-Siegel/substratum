#ifndef SYMTAB_VARIABLE_H
#define SYMTAB_VARIABLE_H

#include "type.h"

#include "ast.h"
#include "symtab_scope.h"

struct VariableEntry
{
    ssize_t stackOffset;
    char *name; // duplicate pointer from ScopeMember for ease of use
    struct Type type;
    // if this variable has the address-of operator used on it or is a global variable
    // we need to denote that it *must* live in memory so it isn't lost
    // and can have an address
    u8 mustSpill;
    u8 isGlobal;
    u8 isExtern;
    u8 isStringLiteral;
};

struct VariableEntry *createVariable(struct Scope *scope,
                                     struct AST *name,
                                     struct Type *type,
                                     u8 isGlobal,
                                     size_t declaredAt,
                                     u8 isArgument,
                                     enum Access accessibility);

void VariableEntry_free(struct VariableEntry *variable);

struct VariableEntry *lookupVarByString(struct Scope *scope,
                                        char *name);

struct VariableEntry *lookupVar(struct Scope *scope,
                                struct AST *name);
#endif
