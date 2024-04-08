#include "regalloc_generic.h"

#include "symtab.h"
#include "tac.h"
#include "util.h"
#include <string.h>

struct Lifetime *newLifetime(char *name, struct Type *type, size_t start, u8 isGlobal, u8 mustSpill)
{
    struct Lifetime *wip = malloc(sizeof(struct Lifetime));
    wip->name = name;
    wip->type = *type;
    wip->start = start;
    wip->end = start;
    wip->stackLocation = 0;
    wip->registerLocation = 0;
    wip->inRegister = 0;
    wip->onStack = 1; // by default, everything gets a slot on the stack
    wip->nwrites = 0;
    wip->nreads = 0;
    wip->isArgument = 0;
    if (isGlobal)
    {
        wip->wbLocation = wb_global;
    }
    else
    {
        if (((type->basicType == vt_class) && (type->indirectionLevel == 0)) || (type->arraySize > 0) || mustSpill)
        {
            wip->wbLocation = wb_stack;
        }
        else
        {
            wip->wbLocation = wb_unknown;
        }
    }
    return wip;
}

int compareLifetimes(struct Lifetime *compared, char *variable)
{
    return strcmp(compared->name, variable);
}

// search through the list of existing lifetimes
// update the lifetime if it exists, insert if it doesn't
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct LinkedList *ltList,
                                        char *name,
                                        struct Type *type,
                                        size_t newEnd,
                                        u8 isGlobal,
                                        u8 mustSpill)
{
    struct Lifetime *thisLt = LinkedList_Find(ltList, &compareLifetimes, name);

    if (thisLt != NULL)
    {
        // this should never fire with well-formed TAC
        // may be helpful when adding/troubleshooting new TAC generation
        if (Type_Compare(&thisLt->type, type))
        {
            char *expectedTypeName = Type_GetName(&thisLt->type);
            char *typeName = Type_GetName(type);
            ErrorAndExit(ERROR_INTERNAL, "Error - type mismatch between identically named variables [%s] expected %s, saw %s!\n", name, expectedTypeName, typeName);
        }
        if (newEnd > thisLt->end)
        {
            thisLt->end = newEnd;
        }
    }
    else
    {
        thisLt = newLifetime(name, type, newEnd, isGlobal, mustSpill);
        LinkedList_Append(ltList, thisLt);
    }

    return thisLt;
}

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct LinkedList *ltList,
                         struct TACOperand *writtenOperand,
                         struct Scope *scope,
                         size_t newEnd)
{
    u8 isGlobal = 0;
    u8 mustSpill = 0;
    if (writtenOperand->permutation == vp_standard)
    {
        struct VariableEntry *recordedVariable = lookupVarByString(scope, writtenOperand->name.str);
        isGlobal = recordedVariable->isGlobal;
        mustSpill = recordedVariable->mustSpill;
    }

    // always use ->type as we don't care what it's cast as to determine its lifetime
    struct Lifetime *updatedLifetime = updateOrInsertLifetime(ltList, writtenOperand->name.str, &(writtenOperand->type), newEnd, isGlobal, mustSpill);
    updatedLifetime->nwrites += 1;
}

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
                        struct TACOperand *readOperand,
                        struct Scope *scope,
                        size_t newEnd)
{
    u8 isGlobal = 0;
    u8 mustSpill = 0;
    if (readOperand->permutation == vp_standard)
    {
        struct VariableEntry *recordedVariable = lookupVarByString(scope, readOperand->name.str);
        isGlobal = recordedVariable->isGlobal;
        mustSpill = recordedVariable->mustSpill;
    }

    // always use ->type as we don't care what it's cast as to determine its lifetime
    struct Lifetime *updatedLifetime = updateOrInsertLifetime(ltList, readOperand->name.str, &(readOperand->type), newEnd, isGlobal, mustSpill);
    updatedLifetime->nreads += 1;
}

struct LinkedList *findLifetimes(struct Scope *scope, struct LinkedList *basicBlockList)
{
    struct LinkedList *lifetimes = LinkedList_New();
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        if (thisMember->type == e_argument)
        {
            struct VariableEntry *theArgument = thisMember->entry;
            struct Lifetime *argLifetime = updateOrInsertLifetime(lifetimes, thisMember->name, &theArgument->type, 0, 0, 0);
            argLifetime->isArgument = 1;
        }
    }

    struct LinkedListNode *blockRunner = basicBlockList->head;
    struct Stack *doDepth = Stack_New();
    while (blockRunner != NULL)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        struct LinkedListNode *TACRunner = thisBlock->TACList->head;
        while (TACRunner != NULL)
        {
            struct TACLine *thisLine = TACRunner->data;
            size_t TACIndex = thisLine->index;

            switch (thisLine->operation)
            {
            case tt_do:
                Stack_Push(doDepth, (void *)(long int)thisLine->index);
                break;

            case tt_enddo:
            {
                size_t extendTo = thisLine->index;
                size_t extendFrom = (size_t)Stack_Pop(doDepth);
                for (struct LinkedListNode *lifetimeRunner = lifetimes->head; lifetimeRunner != NULL; lifetimeRunner = lifetimeRunner->next)
                {
                    struct Lifetime *examinedLifetime = lifetimeRunner->data;
                    if (examinedLifetime->end >= extendFrom && examinedLifetime->end < extendTo)
                    {
                        if (examinedLifetime->name[0] != '.')
                        {
                            examinedLifetime->end = extendTo + 1;
                        }
                    }
                }
            }
            break;

            case tt_asm:
                break;

            case tt_call:
                if (TAC_GetTypeOfOperand(thisLine, 0)->basicType != vt_null)
                {
                    recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
                }
                break;

            case tt_assign:
            {
                recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
                switch (thisLine->operands[1].permutation)
                {
                case vp_standard:
                case vp_temp:
                    recordVariableRead(lifetimes, &thisLine->operands[1], scope, TACIndex);
                    break;

                case vp_literal:
                    break;
                }
            }
            break;

            // single operand in slot 0
            case tt_return:
            case tt_stack_store:
            {
                if (TAC_GetTypeOfOperand(thisLine, 0)->basicType != vt_null)
                {
                    switch (thisLine->operands[0].permutation)
                    {
                    case vp_standard:
                    case vp_temp:
                        recordVariableRead(lifetimes, &thisLine->operands[0], scope, TACIndex);
                        break;

                    case vp_literal:
                        break;
                    }
                }
            }
            break;

            case tt_add:
            case tt_subtract:
            case tt_mul:
            case tt_div:
            case tt_modulo:
            case tt_bitwise_and:
            case tt_bitwise_or:
            case tt_bitwise_xor:
            case tt_bitwise_not:
            case tt_lshift:
            case tt_rshift:
            {
                if (TAC_GetTypeOfOperand(thisLine, 0)->basicType != vt_null)
                {
                    recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
                }

                for (u8 operandIndex = 1; operandIndex < 4; operandIndex++)
                {
                    // lifetimes for every permutation except literal
                    if (thisLine->operands[operandIndex].permutation != vp_literal)
                    {
                        // and any type except null
                        switch (TAC_GetTypeOfOperand(thisLine, operandIndex)->basicType)
                        {
                        case vt_null:
                            break;

                        default:
                            recordVariableRead(lifetimes, &thisLine->operands[operandIndex], scope, TACIndex);
                            break;
                        }
                    }
                }
            }
            break;

            case tt_load:
            case tt_load_off:
            case tt_load_arr:
            case tt_store:
            case tt_store_off:
            case tt_store_arr:
            case tt_addrof:
            case tt_lea_off:
            case tt_lea_arr:
            {
                for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
                {
                    // lifetimes for every permutation except literal
                    if (thisLine->operands[operandIndex].permutation != vp_literal)
                    {
                        // and any type except null
                        switch (TAC_GetTypeOfOperand(thisLine, operandIndex)->basicType)
                        {
                        case vt_null:
                            break;

                        default:
                            recordVariableRead(lifetimes, &thisLine->operands[operandIndex], scope, TACIndex);
                            break;
                        }
                    }
                }
            }
            break;

            case tt_beq:
            case tt_bne:
            case tt_bgeu:
            case tt_bltu:
            case tt_bgtu:
            case tt_bleu:
            case tt_beqz:
            case tt_bnez:
            {
                for (u8 operandIndex = 1; operandIndex < 3; operandIndex++)
                {
                    // lifetimes for every permutation except literal
                    if (thisLine->operands[operandIndex].permutation != vp_literal)
                    {
                        // and any type except null
                        switch (TAC_GetTypeOfOperand(thisLine, operandIndex)->basicType)
                        {
                        case vt_null:
                            break;

                        default:
                            recordVariableRead(lifetimes, &thisLine->operands[operandIndex], scope, TACIndex);
                            break;
                        }
                    }
                }
            }
            break;

            case tt_jmp:
            case tt_label:
            case tt_stack_reserve:
                break;
            }
            TACRunner = TACRunner->next;
        }
        blockRunner = blockRunner->next;
    }

    Stack_Free(doDepth);

    return lifetimes;
}

// populate a linkedlist array so that the list at index i contains all lifetimes active at TAC index i
// then determine which variables should be spilled
size_t generateLifetimeOverlaps(struct CodegenMetadata *metadata)
{
    size_t mostConcurrentLifetimes = 0;

    // populate the array of active lifetimes
    for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *thisLifetime = runner->data;

        for (size_t liveIndex = thisLifetime->start; liveIndex <= thisLifetime->end; liveIndex++)
        {
            LinkedList_Append(metadata->lifetimeOverlaps[liveIndex], thisLifetime);
            if (metadata->lifetimeOverlaps[liveIndex]->size > mostConcurrentLifetimes)
            {
                mostConcurrentLifetimes = metadata->lifetimeOverlaps[liveIndex]->size;
            }
        }
    }

    return mostConcurrentLifetimes;
}
