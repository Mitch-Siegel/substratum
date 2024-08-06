#include "symtab_basicblock.h"

#include "log.h"

struct BasicBlock *basic_block_new(ssize_t labelNum)
{
    struct BasicBlock *wip = malloc(sizeof(struct BasicBlock));
    wip->successors = set_new(free, ssizet_compare);
    wip->TACList = list_new((void (*)(void *))free_tac, NULL);
    wip->labelNum = labelNum;
    return wip;
}

void basic_block_add_successor(struct BasicBlock *block, ssize_t successor)
{
    ssize_t *successorPtr = malloc(sizeof(ssize_t));
    *successorPtr = successor;
    if (!set_try_insert(block->successors, successorPtr))
    {
        free(successorPtr);
    }
}

void basic_block_free(struct BasicBlock *block)
{
    set_free(block->successors);
    list_free(block->TACList);
    free(block);
}

void basic_block_append(struct BasicBlock *block, struct TACLine *line, size_t *tacIndex)
{
    if (tac_line_is_jump(line))
    {
        ssize_t branchTarget = tac_get_jump_target(line);
        log(LOG_DEBUG, "Adding branch target %zd to block %zd", branchTarget, block->labelNum);
        basic_block_add_successor(block, branchTarget);
    }

    line->index = (*tacIndex)++;
    list_append(block->TACList, line);
    char *sprintedAddedLine = sprint_tac_line(line);
    log(LOG_DEBUG, "Append TAC %s", sprintedAddedLine);
    free(sprintedAddedLine);
}

void basic_block_prepend(struct BasicBlock *block, struct TACLine *line)
{
    if (block->TACList->size > 0)
    {
        struct TACLine *first = block->TACList->head->data;
        if (line->index != first->index)
        {
            InternalError("BasicBlock_prepend called with line index %zu - must be %zu (same as start of block!)!", line->index, first->index);
        }
    }

    if (tac_line_is_jump(line))
    {
        ssize_t branchTarget = tac_get_jump_target(line);
        log(LOG_DEBUG, "Adding branch target %zd to block %zd", branchTarget, block->labelNum);
        basic_block_add_successor(block, branchTarget);
    }

    list_prepend(block->TACList, line);
}

void print_basic_block(struct BasicBlock *block, size_t indentLevel)
{
    for (size_t indentPrint = 0; indentPrint < indentLevel; indentPrint++)
    {
        printf("\t");
    }
    printf("BASIC BLOCK %zu\n", block->labelNum);

    Iterator *tacRunner = NULL;
    for (tacRunner = list_begin(block->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
    {
        struct TACLine *thisLine = iterator_get(tacRunner);
        for (size_t indentPrint = 0; indentPrint < indentLevel; indentPrint++)
        {
            printf("\t");
        }

        print_tac_line(thisLine);
        printf("\n");
    }
    iterator_free(tacRunner);
    printf("\n");
}
