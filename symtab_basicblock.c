#include "symtab_basicblock.h"

struct BasicBlock *BasicBlock_new(size_t labelNum)
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
