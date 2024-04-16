#include "symtab_function.h"

#include "util.h"
#include <stddef.h>

#include "symtab_scope.h"

struct FunctionEntry *FunctionEntry_new(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType)
{
    struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
    newFunction->arguments = Stack_New();
    newFunction->argStackSize = 0;
    newFunction->mainScope = Scope_new(parentScope, nameTree->value, newFunction);
    newFunction->BasicBlockList = LinkedList_New();
    newFunction->correspondingTree = *nameTree;
    newFunction->mainScope->parentFunction = newFunction;
    newFunction->returnType = *returnType;
    newFunction->name = nameTree->value;
    newFunction->isDefined = 0;
    newFunction->isAsmFun = 0;
    newFunction->callsOtherFunction = 0;
    return newFunction;
}

void FunctionEntry_free(struct FunctionEntry *function)
{
    Stack_Free(function->arguments);
    LinkedList_Free(function->BasicBlockList, NULL);
    Scope_free(function->mainScope);
    free(function);
}

struct FunctionEntry *lookupFunByString(struct Scope *scope, char *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name);
    if (lookedUp == NULL)
    {
        ErrorAndExit(ERROR_INTERNAL, "Lookup of undeclared function '%s'\n", name);
    }

    switch (lookedUp->type)
    {
    case e_function:
        return lookedUp->entry;

    default:
        ErrorAndExit(ERROR_INTERNAL, "Lookup returned unexpected symbol table entry type when looking up function!\n");
    }
}

struct FunctionEntry *lookupFun(struct Scope *scope, struct AST *name)
{
    struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        ErrorWithAST(ERROR_CODE, name, "Use of undeclared function '%s'\n", name->value);
    }
    switch (lookedUp->type)
    {
    case e_function:
        return lookedUp->entry;

    default:
        ErrorAndExit(ERROR_INTERNAL, "Lookup returned unexpected symbol table entry type when looking up function!\n");
    }
}
