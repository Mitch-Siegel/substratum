#include "symtab_function.h"

#include "log.h"
#include "util.h"
#include <stddef.h>

#include "symtab_basicblock.h"
#include "symtab_scope.h"
#include "symtab_variable.h"

struct FunctionEntry *function_entry_new(struct Scope *parentScope, struct Ast *nameTree, struct TypeEntry *implementedFor)
{
    struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
    memset(newFunction, 0, sizeof(struct FunctionEntry));
    newFunction->arguments = deque_new(NULL);
    newFunction->mainScope = scope_new(parentScope, nameTree->value, newFunction, implementedFor);
    newFunction->BasicBlockList = list_new(NULL, NULL);
    newFunction->correspondingTree = *nameTree;
    newFunction->mainScope->parentFunction = newFunction;
    type_init(&newFunction->returnType);
    newFunction->name = nameTree->value;
    newFunction->implementedFor = implementedFor;
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
    type_deinit(&function->returnType);

    if (function->regalloc.allLifetimes != NULL)
    {
        set_free(function->regalloc.allLifetimes);
        set_free(function->regalloc.touchedRegisters);
    }

    free(function);
}

void print_graphviz_string(char *str, FILE *outFile)
{
    while (*str != '\0')
    {
        switch (*str)
        {
        case '\n':
            fprintf(outFile, "\\n");
            break;
        case '\t':
            fprintf(outFile, "\\t");
            break;
        case '\"':
            fprintf(outFile, "\\\"");
            break;
        case '\\':
            fprintf(outFile, "\\\\");
            break;
        case '>':
            fprintf(outFile, "&gt;");
            break;
        case '<':
            fprintf(outFile, "&lt;");
            break;
        default:
            fprintf(outFile, "%c", *str);
            break;
        }
        str++;
    }
}

void function_entry_print_cfg(struct FunctionEntry *function, FILE *outFile)
{
    fprintf(outFile, "digraph %s_cfg {\n", function->name);
    fprintf(outFile, "graph [splines=ortho];\n");
    fprintf(outFile, "constraint=false;\n");
    fprintf(outFile, "node [shape=record];\n");
    fprintf(outFile, "entry [label=\"entry\", style=\"rounded, filled\";];\n");

    fprintf(outFile, "subgraph cluster_%zd {\n", FUNCTION_EXIT_BLOCK_LABEL);
    fprintf(outFile, "\tstyle=\"invis\";\n");
    fprintf(outFile, "%zd [label=\"exit\"; style=\"rounded, filled\"]\n", FUNCTION_EXIT_BLOCK_LABEL);
    fprintf(outFile, "}\n");

    if (function->BasicBlockList->size == 0)
    {
        fprintf(outFile, "entry -> %zd:0;\n", FUNCTION_EXIT_BLOCK_LABEL);
        return;
    }
    else
    {
        fprintf(outFile, "entry -> %zd:0;\n", ((ssize_t)1));
    }

    Iterator *blockIter = NULL;
    for (blockIter = list_begin(function->BasicBlockList); iterator_gettable(blockIter); iterator_next(blockIter))
    {
        struct BasicBlock *block = iterator_get(blockIter);
        if (block->labelNum == FUNCTION_EXIT_BLOCK_LABEL)
        {
            continue;
        }

        fprintf(outFile, "%zu [rankdir=\"LR\"; label=\"{<0>Basic Block %zu|\n", block->labelNum, block->labelNum);
        Iterator *tacIter = NULL;
        size_t fakeIdx = 0;
        for (tacIter = list_begin(block->TACList); iterator_gettable(tacIter); iterator_next(tacIter))
        {
            struct TACLine *line = iterator_get(tacIter);

            char *sprintedLine = sprint_tac_line(line);
            fprintf(outFile, "<%zu>", line->index);
            print_graphviz_string(sprintedLine, outFile);
            fprintf(outFile, "|\n");
            free(sprintedLine);
        }
        iterator_free(tacIter);
        fprintf(outFile, "}\"];\n");

        fakeIdx = 0;
        for (tacIter = list_begin(block->TACList); iterator_gettable(tacIter); iterator_next(tacIter))
        {
            struct TACLine *line = iterator_get(tacIter);
            if (tac_line_is_jump(line))
            {
                ssize_t branchTarget = tac_get_jump_target(line);
                fprintf(outFile, "\t%zd:%zd -> %zd:0 [penwidth=2];\n", block->labelNum, line->index, branchTarget);
            }
            fakeIdx++;
        }
        iterator_free(tacIter);
    }
    iterator_free(blockIter);

    fprintf(outFile, "}\n");
}

char *sprint_function_signature(struct FunctionEntry *function)
{
    char *funcName = NULL;
    if (function->returnType.basicType != VT_NULL)
    {
        funcName = type_get_name(&function->returnType);
        funcName = realloc(funcName, strlen(funcName) + 2);
        strcat(funcName, " ");
    }
    else
    {
        funcName = strdup("");
    }

    funcName = realloc(funcName, strlen(funcName) + strlen(function->name) + 2);
    strcat(funcName, function->name);
    strcat(funcName, "(");

    for (size_t argIndex = 0; argIndex < function->arguments->size; argIndex++)
    {
        if (argIndex > 0)
        {
            funcName = realloc(funcName, strlen(funcName) + 3);
            strcat(funcName, ", ");
        }

        struct VariableEntry *argument = deque_at(function->arguments, argIndex);
        char *argType = type_get_name(&argument->type);

        funcName = realloc(funcName, strlen(funcName) + strlen(argType) + strlen(argument->name) + 3);
        strcat(funcName, argType);
        strcat(funcName, " ");
        strcat(funcName, argument->name);
        free(argType);
    }
    funcName = realloc(funcName, strlen(funcName) + 2);
    strcat(funcName, ")");

    return funcName;
}

ssize_t function_entry_compare(void *dataA, void *dataB)
{
    struct FunctionEntry *funcA = dataA;
    struct FunctionEntry *funcB = dataB;

    ssize_t diff = strcmp(funcA->name, funcB->name);
    if (diff != 0)
    {
        return diff;
    }

    // check that if a prototype declaration exists, that our parsed declaration matches it exactly
    diff = type_compare(&funcA->returnType, &funcB->returnType);
    if (diff != 0)
    {
        return diff;
    }

    // ensure we have both the same number of bytes of arguments and same number of arguments
    if (funcA->arguments->size == funcB->arguments->size)
    {
        // if we have same number of bytes and same number, ensure everything is exactly the same
        for (size_t argIndex = 0; argIndex < funcA->arguments->size; argIndex++)
        {
            struct VariableEntry *existingArg = deque_at(funcA->arguments, argIndex);
            struct VariableEntry *parsedArg = deque_at(funcB->arguments, argIndex);
            // ensure all arguments in order have same name, type, indirection level
            diff = strcmp(existingArg->name, parsedArg->name);
            if (!diff)
            {
                diff = type_compare(&existingArg->type, &parsedArg->type);
            }

            if (diff)
            {
                break;
            }
        }
    }
    else
    {
        diff = funcA->arguments->size - funcB->arguments->size;
    }

    return diff;
}

void function_entry_print(struct FunctionEntry *function, bool printTac, size_t depth, FILE *outFile)
{
    for (size_t i = 0; i < depth; i++)
    {
        fprintf(outFile, "  ");
    }
    char *signature = sprint_function_signature(function);
    fprintf(outFile, "%s (defined: %d)\n", signature, function->isDefined);
    free(signature);
    scope_print(function->mainScope, outFile, depth, printTac);
}
