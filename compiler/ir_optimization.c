#include "ir_optimization.h"

#include "symtab.h"

// given a LinkedListNode of a TACLine, remove it from the list
// then, renumber all TAC indices afterwards to be index n-1 to account for deletion
// return the next node in the list
struct LinkedListNode *deleteTACFromList(struct LinkedList *tacList, struct LinkedListNode *tacNodeToRemove)
{
    struct LinkedListNode *afterRemoved = tacNodeToRemove->next;
    for(struct LinkedListNode *renumberRunner = afterRemoved; renumberRunner != NULL; renumberRunner = renumberRunner->next)
    {
        struct TACLine *toRenumber = renumberRunner->data;
        toRenumber->index--;
    }

    freeTAC(tacNodeToRemove->data);
    LinkedList_DeleteNode(tacList, tacNodeToRemove);

    return afterRemoved;
}

void peepholeReduceTemps(struct BasicBlock *block)
{
    struct LinkedListNode *prevNode = NULL;
    struct LinkedListNode *blockRunner = block->TACList->head;
    while (blockRunner != NULL)
    {
        struct TACLine *curLine = blockRunner->data;

        if(blockRunner->next == blockRunner)
        {
            ErrorAndExit(ERROR_INTERNAL, "BIG ISSUE\n");
        }

        if ((prevNode == NULL) ||
            (curLine->operation != tt_assign))
        {
            prevNode = blockRunner;
            blockRunner = blockRunner->next;
            continue;
        }

        struct TACLine *prevLine = prevNode->data;

        if ((TAC_GetTypeOfOperand(prevLine, 0) != vt_null) &&
            (TAC_GetTypeOfOperand(curLine, 1) != vt_null) &&
            (!TACOperand_Compare(&prevLine->operands[0], &curLine->operands[1])))
        {
            prevLine->operands[0] = curLine->operands[0];
            blockRunner = deleteTACFromList(block->TACList, blockRunner);
        }
        else
        {
            // no deletion of TAC line, run along normally
            prevNode = blockRunner;
            blockRunner = blockRunner->next;
        }
    }
}

void optimizeIRForBasicBlock(struct Scope *scope, struct BasicBlock *block)
{
    peepholeReduceTemps(block);
}

void optimizeIRForScope(struct Scope *scope)
{
    // second pass: rename basic block operands relevant to the current scope
    for (int i = 0; i < scope->entries->size; i++)
    {
        struct ScopeMember *thisMember = scope->entries->data[i];
        switch (thisMember->type)
        {
        case e_function:
        {
            struct FunctionEntry *theFunction = thisMember->entry;
            optimizeIRForScope(theFunction->mainScope);
        }
        break;

        case e_scope:
        {
            struct Scope *theScope = thisMember->entry;
            optimizeIRForScope(theScope);
        }
        break;

        case e_basicblock:
        {
            struct BasicBlock *theBlock = thisMember->entry;
            optimizeIRForBasicBlock(scope, theBlock);
        }
        break;

        case e_variable:
        case e_argument:
        case e_class:
            break;
        }
    }
}

void optimizeIR(struct SymbolTable *table)
{
    optimizeIRForScope(table->globalScope);
}