#include "symtab_function.h"

#include "log.h"
#include "util.h"
#include <stddef.h>

#include "symtab_scope.h"

struct FunctionEntry *function_entry_new(struct Scope *parentScope, struct Ast *nameTree, struct Type *returnType, struct StructEntry *methodOf)
{
    struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
    memset(newFunction, 0, sizeof(struct FunctionEntry));
    newFunction->arguments = deque_new(NULL);
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
    deque_free(function->arguments);
    linked_list_free(function->BasicBlockList, NULL);
    scope_free(function->mainScope);

    if (function->regalloc.allLifetimes != NULL)
    {
        old_set_free(function->regalloc.allLifetimes);
        old_set_free(function->regalloc.touchedRegisters);
    }

    free(function);
}
