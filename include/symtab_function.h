#ifndef SYMTAB_FUNCTION_H
#define SYMTAB_FUNCTION_H

#include "substratum_defs.h"

#include "ast.h"
#include "regalloc_generic.h"
#include "symtab_scope.h"
#include "type.h"

#include "mbcl/deque.h"
#include "mbcl/list.h"

struct FunctionEntry
{
    struct Type returnType;
    struct Scope *mainScope;
    Deque *arguments;             // stack of VariableEntry pointers corresponding by index to arguments
    char *name;                   // duplicate pointer from ScopeMember for ease of use
    struct StructEntry *methodOf; // if this function is a member of a struct, points to which struct
    List *BasicBlockList;
    struct Ast correspondingTree;
    u8 isDefined;
    u8 isAsmFun;
    u8 callsOtherFunction; // is it possible this function calls another function? (need to store return address on stack)
    u8 isMethod;           // if memberOf != null and this is true, the function is a method (takes a 'self' parameter)
    struct RegallocMetadata regalloc;
};

struct FunctionEntry *function_entry_new(struct Scope *parentScope, struct Ast *nameTree, struct Type *returnType, struct StructEntry *methodOf);

void function_entry_free(struct FunctionEntry *function);

void function_entry_print_cfg(struct FunctionEntry *function, FILE *outFile);

#endif
