#include "ir_optimization.h"

#include "symtab.h"

// given a LinkedListNode of a TACLine, remove it from the list
// then, renumber all TAC indices afterwards to be index n-1 to account for deletion
// return the next node in the list
struct LinkedListNode *deleteTACFromList(struct LinkedList *tacList, struct LinkedListNode *tacNodeToRemove)
{
    struct LinkedListNode *afterRemoved = tacNodeToRemove->next;
    for (struct LinkedListNode *renumberRunner = afterRemoved; renumberRunner != NULL; renumberRunner = renumberRunner->next)
    {
        struct TACLine *toRenumber = renumberRunner->data;
        toRenumber->index--;
    }

    freeTAC(tacNodeToRemove->data);
    LinkedList_DeleteNode(tacList, tacNodeToRemove);

    return afterRemoved;
}

/*
 * peephole optimization to eliminiate use of extra temp variables
 * TAC combos such as
 * .t0 = a + b
 * a = .t0
 * convert to: a = a + b
 */
void peepholeReduceTemps(struct BasicBlock *block)
{
    struct LinkedListNode *prevNode = NULL;
    struct LinkedListNode *blockRunner = block->TACList->head;
    while (blockRunner != NULL)
    {
        struct TACLine *curLine = blockRunner->data;

        if (blockRunner->next == blockRunner)
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

/*
 * eliminate common subexpressions (re-calculation of temp variables when not necessary)
 *
 */
struct Subexpression
{
    struct Stack *dependsOn;
    struct TACOperand *provides;
};

void Subexpression_Free(struct Subexpression *s)
{
    Stack_Free(s->dependsOn);
    free(s);
}

int Subexpression_Compare(struct Subexpression *a, struct Subexpression *b)
{
    return TACOperand_Compare(a->provides, b->provides);
}

int Subexpression_DependsOn(struct Subexpression *s, struct TACOperand *o)
{
    for (int i = 0; i < s->dependsOn->size; i++)
    {
        struct TACOperand *examined = (struct TACOperand *)s->dependsOn->data[i];
        if (!TACOperand_Compare(examined, o))
        {
            return 1;
        }
    }

    return 0;
}

void invalidateOldExpressions(struct TACOperand *newlyAssigned, struct LinkedList *activeExpressions)
{
    struct LinkedListNode *expRunner = activeExpressions->head;
    while (expRunner != NULL)
    {
        struct Subexpression *theSubex = expRunner->data;

        if (Subexpression_DependsOn(theSubex, newlyAssigned))
        {
            struct LinkedListNode *next = expRunner->next;
            printf("invalidates %s\n", theSubex->provides->name.str);
            free(expRunner->data);
            LinkedList_DeleteNode(activeExpressions, expRunner);
            expRunner = next;
        }
        else
        {
            expRunner = expRunner->next;
        }
    }
}

int addNewlyCalculatedSubexpression(struct TACLine *expression, struct LinkedList *activeExpressions)
{
    struct Subexpression *newSub = malloc(sizeof(struct Subexpression));
    newSub->dependsOn = Stack_New();
    newSub->provides = &expression->operands[0];

    struct Subexpression *existingSub = LinkedList_Find(activeExpressions, Subexpression_Compare, newSub);
    if (existingSub != NULL)
    {
        Subexpression_Free(LinkedList_FindAndDelete(activeExpressions, Subexpression_Compare, newSub));
        printf("duplicate found!\n");
    }
    else
    {
        for (int i = 1; i < 4; i++)
        {
            if ((TACOperand_GetType(&expression->operands[i])->basicType != vt_null) &&
                ((expression->operands[i].permutation == vp_standard) ||
                 (expression->operands[i].permutation == vp_temp)) &&
                !Subexpression_DependsOn(newSub, &expression->operands[i]))
            {
                struct Subexpression *recursiveSub = LinkedList_Find(activeExpressions, Subexpression_Compare, &expression->operands[i]);
                if (recursiveSub != NULL)
                {
                    for (int j = 0; j < recursiveSub->dependsOn->size; j++)
                    {
                        struct TACOperand *recursiveDep = (struct TACOperand *)recursiveSub->dependsOn->data[i];
                        if (!Subexpression_DependsOn(newSub, recursiveDep))
                        {
                            printf("\tadd dep %s\n", recursiveDep->name.str);
                            Stack_Push(newSub->dependsOn, recursiveDep);
                        }
                    }
                }
                else
                {
                    Stack_Push(newSub->dependsOn, &expression->operands[i]);
                }
            }
        }
    }
    invalidateOldExpressions(newSub->provides, activeExpressions);
    LinkedList_Append(activeExpressions, newSub);

    // struct LinkedListNode *expRunner = activeExpressions->head;
    // while (expRunner != NULL)
    // {
    // struct Subexpression *theSubex = expRunner->data;
    // if (!AST_Compare(theSubex->exprTree, newSub->exprTree))
    // {
    // printTACLine(expression);
    // printf("is redundant\n");
    // free(newSub);
    // return 1;
    // }/

    // expRunner = expRunner->next;
    // }

    // LinkedList_Append(activeExpressions, newSub);

    return 0;
}

void eliminiateCommonSubexpressions(struct BasicBlock *block)
{
    struct LinkedList *activeExpressions = LinkedList_New();

    struct LinkedListNode *blockRunner = block->TACList->head;
    while (blockRunner != NULL)
    {
        struct TACLine *curLine = blockRunner->data;

        if (curLine->operands[0].permutation != vp_temp)
        {
            blockRunner = blockRunner->next;
            continue;
        }

        switch (curLine->operation)
        {
        case tt_assign:
        case tt_add:
        case tt_subtract:
        case tt_mul:
        case tt_div:
        case tt_load:
        case tt_load_off:
        case tt_load_arr:
        case tt_store:
        case tt_store_off:
        case tt_store_arr:
        case tt_addrof:
        case tt_lea_off:
        case tt_lea_arr:
            printTACLine(curLine);
            printf("\n");
            if (addNewlyCalculatedSubexpression(curLine, activeExpressions))
            {
                printf("can eliminate something\n");
            }
            break;

        default:
            break;
        }

        blockRunner = blockRunner->next;
    }

    LinkedList_Free(activeExpressions, Subexpression_Free);
}

void optimizeIRForBasicBlock(struct Scope *scope, struct BasicBlock *block)
{
    // peepholeReduceTemps(block);
    eliminiateCommonSubexpressions(block);
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