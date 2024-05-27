#include "symtab_function.h"

#include "log.h"
#include "util.h"
#include <stddef.h>

#include "symtab_scope.h"

struct FunctionEntry *FunctionEntry_new(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType)
{
    struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
    memset(newFunction, 0, sizeof(struct FunctionEntry));
    newFunction->arguments = Stack_New();
    newFunction->mainScope = Scope_new(parentScope, nameTree->value, newFunction);
    newFunction->BasicBlockList = LinkedList_New();
    newFunction->correspondingTree = *nameTree;
    newFunction->mainScope->parentFunction = newFunction;
    newFunction->returnType = *returnType;
    newFunction->name = nameTree->value;
    newFunction->methodOf = NULL;
    newFunction->isDefined = 0;
    newFunction->isAsmFun = 0;
    newFunction->callsOtherFunction = 0;

    newFunction->regalloc.function = newFunction;
    newFunction->regalloc.scope = newFunction->mainScope;

    return newFunction;
}

void FunctionEntry_free(struct FunctionEntry *function)
{
    Stack_Free(function->arguments);
    LinkedList_Free(function->BasicBlockList, NULL);
    Scope_free(function->mainScope);

    if (function->regalloc.allLifetimes != NULL)
    {
        Set_Free(function->regalloc.allLifetimes);
        Set_Free(function->regalloc.touchedRegisters);
    }

    free(function);
}

// create a new function accessible within the given scope
struct FunctionEntry *createFunction(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType, enum Access accessibility)
{
    struct FunctionEntry *newFunction = FunctionEntry_new(parentScope, nameTree, returnType);
    Scope_insert(parentScope, nameTree->value, newFunction, e_function, accessibility);
    return newFunction;
}

struct FunctionEntry *lookupFunByString(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name);
    if (lookedUp == NULL)
    {
        InternalError("Lookup of undeclared function '%s'", name);
    }

    switch (lookedUp->type)
    {
    case e_function:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up function!");
    }
}

struct FunctionEntry *lookupFun(struct Scope *scope, struct AST *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        LogTree(LOG_FATAL, name, "Use of undeclared function '%s'", name->value);
    }
    switch (lookedUp->type)
    {
    case e_function:
        return lookedUp->entry;

    default:
        InternalError("Lookup returned unexpected symbol table entry type when looking up function!");
    }
}
