#ifndef SYMTAB_FUNCTION_H
#define SYMTAB_FUNCTION_H

#include "substratum_defs.h"

#include "ast.h"
#include "type.h"

struct FunctionEntry
{
    size_t argStackSize;
    struct Type returnType;
    struct Scope *mainScope;
    struct Stack *arguments; // stack of VariableEntry pointers corresponding by index to arguments
    char *name;              // duplicate pointer from ScopeMember for ease of use
    struct LinkedList *BasicBlockList;
    struct AST correspondingTree;
    u8 isDefined;
    u8 isAsmFun;
    u8 callsOtherFunction; // is it possible this function calls another function? (need to store return address on stack)
};

struct FunctionEntry *FunctionEntry_new(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType);

void FunctionEntry_free(struct FunctionEntry *function);

struct FunctionEntry *createFunction(struct Scope *parentScope,
                                     struct AST *nameTree,
                                     struct Type *returnType);

struct FunctionEntry *lookupFunByString(struct Scope *scope,
                                        char *name);

struct FunctionEntry *lookupFun(struct Scope *scope,
                                struct AST *name);

#endif
