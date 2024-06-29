#include "symtab_function.h"

#include "log.h"
#include "util.h"
#include <stddef.h>

#include "symtab_scope.h"

struct FunctionEntry *function_entry_new(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType, struct StructEntry *methodOf)
{
    struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
    memset(newFunction, 0, sizeof(struct FunctionEntry));
    newFunction->arguments = stack_new();
    newFunction->mainScope = scope_new(parentScope, nameTree->value, newFunction, methodOf);
    newFunction->BasicBlockList = linked_list_new();
    newFunction->correspondingTree = *nameTree;
    newFunction->mainScope->parentFunction = newFunction;
    newFunction->returnType = *returnType;
    newFunction->name = nameTree->value;
    newFunction->methodOf = methodOf;
    newFunction->isDefined = 0;
    newFunction->isAsmFun = 0;
    newFunction->callsOtherFunction = 0;

    newFunction->regalloc.function = newFunction;
    newFunction->regalloc.scope = newFunction->mainScope;

    return newFunction;
}

void function_entry_free(struct FunctionEntry *function)
{
    stack_free(function->arguments);
    linked_list_free(function->BasicBlockList, NULL);
    scope_free(function->mainScope);

    if (function->regalloc.allLifetimes != NULL)
    {
        set_free(function->regalloc.allLifetimes);
        set_free(function->regalloc.touchedRegisters);
    }

    free(function);
}

// create a new function accessible within the given scope
struct FunctionEntry *create_function(struct Scope *parentScope,
                                     struct AST *nameTree,
                                     struct Type *returnType,
                                     struct StructEntry *methodOf,
                                     enum ACCESS accessibility)
{
    struct FunctionEntry *newFunction = function_entry_new(parentScope, nameTree, returnType, methodOf);
    scope_insert(parentScope, nameTree->value, newFunction, E_FUNCTION, accessibility);
    return newFunction;
}
