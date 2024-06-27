#include "symtab_basicblock.h"

#include "log.h"

struct BasicBlock *basic_block_new(ssize_t labelNum)
{
    struct BasicBlock *wip = malloc(sizeof(struct BasicBlock));
    wip->TACList = linked_list_new();
    wip->labelNum = labelNum;
    return wip;
}

void basic_block_free(struct BasicBlock *block)
{
    linked_list_free(block->TACList, free_tac);
    free(block);
}

void basic_block_append(struct BasicBlock *block, struct TACLine *line, size_t *tacIndex)
{
    line->index = (*tacIndex)++;
    linked_list_append(block->TACList, line);
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

    linked_list_prepend(block->TACList, line);
}

void print_basic_block(struct BasicBlock *block, size_t indentLevel)
{
    for (size_t indentPrint = 0; indentPrint < indentLevel; indentPrint++)
    {
        printf("\t");
    }
    printf("BASIC BLOCK %zu\n", block->labelNum);
    for (struct LinkedListNode *runner = block->TACList->head; runner != NULL; runner = runner->next)
    {
        struct TACLine *this = runner->data;
        for (size_t indentPrint = 0; indentPrint < indentLevel; indentPrint++)
        {
            printf("\t");
        }

        if (runner->data != NULL)
        {
            print_tac_line(this);
            printf("\n");
        }
    }
    printf("\n");
}
