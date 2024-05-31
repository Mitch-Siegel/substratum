#include "regalloc_generic.h"

#include "log.h"
#include "symtab.h"
#include "tac.h"
#include "util.h"
#include <string.h>

struct MachineInfo *(*setupMachineInfo)() = NULL;

struct MachineInfo *MachineInfo_New(u8 maxReg,
                                    u8 n_temps,
                                    u8 n_arguments,
                                    u8 n_general_purpose,
                                    u8 n_no_save,
                                    u8 n_callee_save,
                                    u8 n_caller_save)
{
    struct MachineInfo *wip = malloc(sizeof(struct MachineInfo));
    memset(wip, 0, sizeof(struct MachineInfo));

    wip->maxReg = maxReg;
    wip->allRegisters = malloc(wip->maxReg * sizeof(struct Register *));
    memset(wip->allRegisters, 0, wip->maxReg * sizeof(struct Register *));

    wip->n_temps = n_temps;
    wip->temps = malloc(wip->n_temps * sizeof(struct Register *));
    memset(wip->temps, 0, wip->n_temps * sizeof(struct Register *));
    wip->tempsOccupied = malloc(wip->n_temps * sizeof(u8));
    memset(wip->tempsOccupied, 0, wip->n_temps * sizeof(u8));

    wip->n_arguments = n_arguments;
    wip->arguments = malloc(wip->n_arguments * sizeof(struct Register *));
    memset(wip->arguments, 0, wip->n_arguments * sizeof(struct Register *));

    wip->n_general_purpose = n_general_purpose;
    wip->generalPurpose = malloc(wip->n_general_purpose * sizeof(struct Register *));
    memset(wip->generalPurpose, 0, wip->n_general_purpose * sizeof(struct Register *));

    wip->n_no_save = n_no_save;
    wip->no_save = malloc(wip->n_no_save * sizeof(struct Register *));
    memset(wip->no_save, 0, wip->n_no_save * sizeof(struct Register *));

    wip->n_callee_save = n_callee_save;
    wip->callee_save = malloc(wip->n_callee_save * sizeof(struct Register *));
    memset(wip->callee_save, 0, wip->n_callee_save * sizeof(struct Register *));

    wip->n_caller_save = n_caller_save;
    wip->caller_save = malloc(wip->n_caller_save * sizeof(struct Register *));
    memset(wip->caller_save, 0, wip->n_caller_save * sizeof(struct Register *));

    return wip;
}

void MachineInfo_Free(struct MachineInfo *info)
{
    free(info->allRegisters);
    free(info->temps);
    free(info->tempsOccupied);
    free(info->generalPurpose);
    free(info->arguments);
    free(info->no_save);
    free(info->callee_save);
    free(info->caller_save);

    free(info);
}

struct Lifetime *Lifetime_New(char *name, struct Type *type, size_t start, u8 isGlobal, u8 mustSpill)
{
    struct Lifetime *wip = malloc(sizeof(struct Lifetime));
    wip->name = name;
    wip->type = *type;
    wip->start = start;
    wip->end = start;
    wip->writebackInfo.stackOffset = 0;
    wip->isArgument = 0;
    wip->nwrites = 0;
    wip->nreads = 0;
    if (isGlobal)
    {
        wip->wbLocation = wb_global;
    }
    else
    {
        if (((type->basicType == vt_struct) && (type->pointerLevel == 0)) ||
            (type->basicType == vt_array) ||
            mustSpill)
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

size_t Lifetime_Hash(struct Lifetime *lifetime)
{
    return hashString(lifetime->name);
}

ssize_t Lifetime_Compare(struct Lifetime *lifetimeA, struct Lifetime *lifetimeB)
{
    return strcmp(lifetimeA->name, lifetimeB->name);
}

bool Lifetime_IsLiveAtIndex(struct Lifetime *lifetime, size_t index)
{
    return ((lifetime->start <= index) && (lifetime->end >= index));
}

// search through the list of existing lifetimes
// update the lifetime if it exists, insert if it doesn't
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct Set *ltList,
                                        char *name,
                                        struct Type *type,
                                        size_t newEnd,
                                        u8 isGlobal,
                                        u8 mustSpill)
{
    struct Lifetime dummyFind = {0};
    dummyFind.name = name;
    struct Lifetime *thisLt = Set_Find(ltList, &dummyFind);

    if (thisLt != NULL)
    {
        // this should never fire with well-formed TAC
        // may be helpful when adding/troubleshooting new TAC generation
        if (Type_Compare(&thisLt->type, type))
        {
            char *expectedTypeName = Type_GetName(&thisLt->type);
            char *typeName = Type_GetName(type);
            InternalError("Type mismatch between identically named variables [%s] expected %s, saw %s!", name, expectedTypeName, typeName);
        }
        if (newEnd > thisLt->end)
        {
            thisLt->end = newEnd;
        }
    }
    else
    {
        Log(LOG_DEBUG, "Create lifetime starting at %zu for %s: global? %d mustspill? %d", newEnd, name, isGlobal, mustSpill);
        thisLt = Lifetime_New(name, type, newEnd, isGlobal, mustSpill);
        Set_Insert(ltList, thisLt);
    }

    return thisLt;
}

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct Set *ltList,
                         struct TACOperand *writtenOperand,
                         struct Scope *scope,
                         size_t newEnd)
{
    Log(LOG_DEBUG, "Record variable write for %s at index %zu", writtenOperand->name.str, newEnd);

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
void recordVariableRead(struct Set *ltList,
                        struct TACOperand *readOperand,
                        struct Scope *scope,
                        size_t newEnd)
{
    Log(LOG_DEBUG, "Record variable read for %s at index %zu", readOperand->name.str, newEnd);

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

void recordLifetimeWriteForOperand(struct Set *lifetimes, struct TACOperand *operand, struct Scope *scope, size_t tacIndex)
{
    if ((TACOperand_GetType(operand)->basicType != vt_null) && (operand->permutation != vp_literal))
    {
        recordVariableWrite(lifetimes, operand, scope, tacIndex);
    }
}

void recordLifetimeReadForOperand(struct Set *lifetimes, struct TACOperand *operand, struct Scope *scope, size_t tacIndex)
{
    if ((TACOperand_GetType(operand)->basicType != vt_null) && (operand->permutation != vp_literal))
    {
        recordVariableRead(lifetimes, operand, scope, tacIndex);
    }
}

void findLifetimesForTac(struct Set *lifetimes, struct Scope *scope, struct TACLine *line, struct Stack *doDepth)
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
        for (struct LinkedListNode *lifetimeRunner = lifetimes->elements->head; lifetimeRunner != NULL; lifetimeRunner = lifetimeRunner->next)
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

void addArgumentLifetimesForScope(struct Set *lifetimes, struct Scope *scope)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        if (thisMember->type == e_argument)
        {
            struct VariableEntry *theArgument = thisMember->entry;
            // arguments can be mustSpill too - if they are used in an address-of it will be required not to ever load them into registers
            updateOrInsertLifetime(lifetimes, thisMember->name, &theArgument->type, 0, 0, theArgument->mustSpill)->isArgument = 1;
        }
    }
}

struct Set *findLifetimes(struct Scope *scope, struct LinkedList *basicBlockList)
{
    struct Set *lifetimes = Set_New((ssize_t(*)(void *, void *))Lifetime_Compare, free);

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

/*
 *
 * Register struct
 *
 */
struct Register *Register_New(u8 index)
{
    struct Register *reg = malloc(sizeof(struct Register));
    reg->containedLifetime = NULL;
    reg->index = index;

    return reg;
}

bool Register_IsLive(struct Register *reg, size_t index)
{
    if (reg->containedLifetime == NULL)
    {
        return false;
    }

    return Lifetime_IsLiveAtIndex(reg->containedLifetime, index);
}
