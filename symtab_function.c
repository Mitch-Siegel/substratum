#include "symtab_function.h"

#include "log.h"
#include "util.h"
#include <stddef.h>

#include "symtab_basicblock.h"
#include "symtab_scope.h"

struct FunctionEntry *function_entry_new(struct Scope *parentScope, struct Ast *nameTree, struct Type *returnType, struct StructEntry *methodOf)
{
    struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
    memset(newFunction, 0, sizeof(struct FunctionEntry));
    newFunction->arguments = deque_new(NULL);
    newFunction->mainScope = scope_new(parentScope, nameTree->value, newFunction, methodOf);
    newFunction->BasicBlockList = list_new(NULL, NULL);
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
    list_free(function->BasicBlockList);
    scope_free(function->mainScope);

    if (function->regalloc.allLifetimes != NULL)
    {
        set_free(function->regalloc.allLifetimes);
        set_free(function->regalloc.touchedRegisters);
    }

    free(function);
}

void function_entry_print_cfg(struct FunctionEntry *function, FILE *outFile)
{
    fprintf(outFile, "digraph %s_cfg {\n", function->name);
    fprintf(outFile, "node [shape=record];\n");
    fprintf(outFile, "entry [label=\"entry\"];\n");
    fprintf(outFile, "%zu [label=\"exit\"];\n", (ssize_t)FUNCTION_EXIT_BLOCK_LABEL);

    if (function->BasicBlockList->size == 0)
    {
        fprintf(outFile, "entry -> %zd;\n", FUNCTION_EXIT_BLOCK_LABEL);
        fprintf(outFile, "%zd -> %zd;\n", FUNCTION_EXIT_BLOCK_LABEL, FUNCTION_EXIT_BLOCK_LABEL);
        return;
    }
    else
    {
        fprintf(outFile, "entry -> %zd;\n", ((ssize_t)1));
    }

    Iterator *blockIter = NULL;
    for (blockIter = list_begin(function->BasicBlockList); iterator_gettable(blockIter); iterator_next(blockIter))
    {
        struct BasicBlock *block = iterator_get(blockIter);
        if (block->labelNum == FUNCTION_EXIT_BLOCK_LABEL)
        {
            continue;
        }
        fprintf(outFile, "%zd [label=\"bb_%zd\"]", block->labelNum, block->labelNum);
        Iterator *successorIter = NULL;
        for (successorIter = set_begin(block->successors); iterator_gettable(successorIter); iterator_next(successorIter))
        {
            ssize_t *targetBlock = iterator_get(successorIter);
            fprintf(outFile, "%zd -> %zd;\n", block->labelNum, *targetBlock);
        }
        iterator_free(successorIter);
    }
    iterator_free(blockIter);
    fprintf(outFile, "}\n");
}
