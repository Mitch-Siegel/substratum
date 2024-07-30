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
    fprintf(outFile, "node [shape=record];\n");
    fprintf(outFile, "entry [label=\"entry\"];\n");

    fprintf(outFile, "subgraph cluster_%zd {\n", FUNCTION_EXIT_BLOCK_LABEL);
    fprintf(outFile, "\tlabel=\"exit\";\n");
    fprintf(outFile, "\tstyle=\"rounded, filled\";\n");
    fprintf(outFile, "bb_%zd_entry [label=\"done\"];\n", FUNCTION_EXIT_BLOCK_LABEL);
    fprintf(outFile, "}\n");

    if (function->BasicBlockList->size == 0)
    {
        fprintf(outFile, "entry -> bb_%zd:0;\n", FUNCTION_EXIT_BLOCK_LABEL);
        return;
    }
    else
    {
        fprintf(outFile, "entry -> bb_%zd:0;\n", ((ssize_t)1));
    }

    Iterator *blockIter = NULL;
    for (blockIter = list_begin(function->BasicBlockList); iterator_gettable(blockIter); iterator_next(blockIter))
    {
        struct BasicBlock *block = iterator_get(blockIter);

        fprintf(outFile, "subgraph cluster_%zd {\n", block->labelNum);
        fprintf(outFile, "penwidth=0;\n");

        fprintf(outFile, "\tbb_%zu [\n", block->labelNum);
        fprintf(outFile, "\t\tshape=record\n");
        fprintf(outFile, "\t\tlabel=<\n");
        fprintf(outFile, "\t\t\t<table border=\"0\" cellborder=\"0\" cellspacing=\"0\">\n");
        fprintf(outFile, "\t\t\t\t<tr>\n");
        fprintf(outFile, "\t\t\t\t\t<td port=\"0\">Basic Block %zu</td>\n", block->labelNum);
        fprintf(outFile, "\t\t\t\t</tr>\n");
        Iterator *tacIter = NULL;
        for (tacIter = list_begin(block->TACList); iterator_gettable(tacIter); iterator_next(tacIter))
        {
            struct TACLine *line = iterator_get(tacIter);

            char *sprintedLine = sprint_tac_line(line);
            fprintf(outFile, "\t\t\t\t<tr>\n");
            fprintf(outFile, "\t\t\t\t\t<td>");
            print_graphviz_string(sprintedLine, outFile);
            fprintf(outFile, "</td>\n");
            fprintf(outFile, "\t\t\t\t\t<td port=\"%zd\">", line->index);
            fprintf(outFile, "</td>\n");
            fprintf(outFile, "\t\t\t\t</tr>\n");
            free(sprintedLine);
        }
        iterator_free(tacIter);
        fprintf(outFile, "\t\t\t</table>\n");
        fprintf(outFile, "\t\t>\n");
        fprintf(outFile, "\t];\n");

        fprintf(outFile, "\t}\n");
        for (tacIter = list_begin(block->TACList); iterator_gettable(tacIter); iterator_next(tacIter))
        {
            struct TACLine *line = iterator_get(tacIter);
            if (tac_line_is_jump(line))
            {
                ssize_t branchTarget = tac_get_jump_target(line);
                fprintf(outFile, "\tbb_%zd:%zd -> bb_%zd:0 [penwidth=2];\n", block->labelNum, line->index, branchTarget);
            }
        }
        iterator_free(tacIter);
    }
    iterator_free(blockIter);

    fprintf(outFile, "}\n");
}
