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

void recordLifetimeWriteForOperand(struct LinkedList *lifetimes, struct TACOperand *operand, struct Scope *scope, size_t tacIndex)
{
    if ((TACOperand_GetType(operand)->basicType != vt_null) && (operand->permutation != vp_literal))
    {
        recordVariableWrite(lifetimes, operand, scope, tacIndex);
    }
}

void recordLifetimeReadForOperand(struct LinkedList *lifetimes, struct TACOperand *operand, struct Scope *scope, size_t tacIndex)
{
    if ((TACOperand_GetType(operand)->basicType != vt_null) && (operand->permutation != vp_literal))
    {
        recordVariableRead(lifetimes, operand, scope, tacIndex);
    }
}

void findLifetimesForTac(struct LinkedList *lifetimes, struct Scope *scope, struct TACLine *line, struct Stack *doDepth)
{
    // handle tt_do/tt_enddo stack and lifetime extension
    switch (line->operation)
    {
    case tt_do:
        Stack_Push(doDepth, (void *)(long int)line->index);
        break;

    case tt_enddo:
    {
        size_t extendTo = line->index;
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

    default:
        break;
    }

    for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
    {
        switch (getUseOfOperand(line, operandIndex))
        {
        case u_unused:
            break;

        case u_read:
            recordLifetimeReadForOperand(lifetimes, &line->operands[operandIndex], scope, line->index);
            break;

        case u_write:
            recordLifetimeWriteForOperand(lifetimes, &line->operands[operandIndex], scope, line->index);
            break;
        }
    }
}

void addArgumentLifetimesForScope(struct LinkedList *lifetimes, struct Scope *scope)
{
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
}

struct LinkedList *findLifetimes(struct Scope *scope, struct LinkedList *basicBlockList)
{
    struct LinkedList *lifetimes = LinkedList_New();

    addArgumentLifetimesForScope(lifetimes, scope);

    struct LinkedListNode *blockRunner = basicBlockList->head;
    struct Stack *doDepth = Stack_New();
    while (blockRunner != NULL)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        struct LinkedListNode *TACRunner = thisBlock->TACList->head;
        while (TACRunner != NULL)
        {
            struct TACLine *thisLine = TACRunner->data;
            findLifetimesForTac(lifetimes, scope, thisLine, doDepth);
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
