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
    bool mustSpill;
    bool isGlobal;
    bool isExtern;
    bool isStringLiteral;
};

struct VariableEntry *variable_entry_new(char *name,
                                         struct Type *type,
                                         bool isGlobal,
                                         bool isArgument,
                                         enum ACCESS accessibility);

void variable_entry_free(struct VariableEntry *variable);
#endif
