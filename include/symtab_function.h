#ifndef SYMTAB_FUNCTION_H
#define SYMTAB_FUNCTION_H

#include "substratum_defs.h"

#include "ast.h"
#include "regalloc_generic.h"
#include "symtab_scope.h"
#include "type.h"

#include "mbcl/deque.h"
#include "mbcl/list.h"

// TODO: associate AST with function entry for line/col traceablility in error messages
struct FunctionEntry
{
    struct Type returnType;
    struct Scope *mainScope;
    Deque *arguments;                 // stack of VariableEntry pointers corresponding by index to arguments
    char *name;                       // duplicate pointer from ScopeMember for ease of use
    struct TypeEntry *implementedFor; // if this function is part of an implementation for a type, points to which type
    List *BasicBlockList;
    struct Ast correspondingTree;
    u8 isDefined;
    u8 isAsmFun;
    u8 callsOtherFunction; // is it possible this function calls another function? (need to store return address on stack)
    u8 isMethod;           // if memberOf != null and this is true, the function is a method (takes a 'self' parameter)
    struct RegallocMetadata regalloc;
};

struct FunctionEntry *function_entry_new(struct Scope *parentScope, struct Ast *nameTree, struct TypeEntry *implementedFor);

void function_entry_free(struct FunctionEntry *function);

void function_entry_print_cfg(struct FunctionEntry *function, FILE *outFile);

char *sprint_function_signature(struct FunctionEntry *function);

ssize_t function_entry_compare(void *dataA, void *dataB);

void function_entry_print(struct FunctionEntry *function, bool printTac, size_t depth, FILE *outFile);

#endif
