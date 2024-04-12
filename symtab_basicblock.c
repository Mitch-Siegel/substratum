#include "symtab_basicblock.h"

struct BasicBlock *BasicBlock_new(ssize_t labelNum)
{
    struct BasicBlock *wip = malloc(sizeof(struct BasicBlock));
    wip->TACList = LinkedList_New();
    wip->labelNum = labelNum;
    return wip;
}

void BasicBlock_free(struct BasicBlock *block)
{
    LinkedList_Free(block->TACList, freeTAC);
    free(block);
}

void BasicBlock_append(struct BasicBlock *block, struct TACLine *line)
{
    LinkedList_Append(block->TACList, line);
}

void BasicBlock_prepend(struct BasicBlock *block, struct TACLine *line)
{
    if (block->TACList->size > 0)
    {
        struct TACLine *first = block->TACList->head->data;
        if (line->index != first->index)
        {
            ErrorAndExit(ERROR_INTERNAL, "BasicBlock_prepend called with line index %zu - must be %zu (same as start of block!)!\n", line->index, first->index);
        }
    }
    else
    {
        ErrorAndExit(ERROR_INTERNAL, "BasicBlock_prepend called on empty block!\n");
    }

    LinkedList_Prepend(block->TACList, line);
}

void printBasicBlock(struct BasicBlock *block, size_t indentLevel)
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
            printTACLine(this);
            printf("\n");
        }
    }
    printf("\n");
}
