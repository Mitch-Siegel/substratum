#include "regalloc_generic.h"

#include "log.h"
#include "symtab.h"
#include "tac.h"
#include "util.h"
#include <string.h>

void *lifetime_find(struct Set *allLifetimes, char *lifetimeName)
{
    struct Lifetime dummy = {0};
    dummy.name = lifetimeName;
    return set_find(allLifetimes, &dummy);
}

struct Lifetime *lifetime_new(char *name, struct Type *type, size_t start, u8 isGlobal, u8 mustSpill)
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
        wip->wbLocation = WB_GLOBAL;
    }
    else
    {
        if (type_is_object(type) || mustSpill)
        {
            wip->wbLocation = WB_STACK;
        }
        else
        {
            wip->wbLocation = WB_UNKNOWN;
        }
    }
    return wip;
}

size_t lifetime_hash(struct Lifetime *lifetime)
{
    return hash_string(lifetime->name);
}

ssize_t lifetime_compare(struct Lifetime *lifetimeA, struct Lifetime *lifetimeB)
{
    return strcmp(lifetimeA->name, lifetimeB->name);
}

bool lifetime_is_live_at_index(struct Lifetime *lifetime, size_t index)
{
    return ((lifetime->start <= index) && (lifetime->end >= index));
}

// search through the list of existing lifetimes
// update the lifetime if it exists, insert if it doesn't
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *update_or_insert_lifetime(struct Set *ltList,
                                           char *name,
                                           struct Type *type,
                                           size_t newEnd,
                                           u8 isGlobal,
                                           u8 mustSpill)
{
    struct Lifetime *thisLt = lifetime_find(ltList, name);

    if (thisLt != NULL)
    {
        // this should never fire with well-formed TAC
        // may be helpful when adding/troubleshooting new TAC generation
        if (type_compare(&thisLt->type, type))
        {
            char *expectedTypename = type_get_name(&thisLt->type);
            char *typename = type_get_name(type);
            InternalError("Type mismatch between identically named variables [%s] expected %s, saw %s!", name, expectedTypename, typename);
        }
        if (newEnd > thisLt->end)
        {
            thisLt->end = newEnd;
        }
    }
    else
    {
        log(LOG_DEBUG, "Create lifetime starting at %zu for %s: global? %d mustspill? %d", newEnd, name, isGlobal, mustSpill);
        thisLt = lifetime_new(name, type, newEnd, isGlobal, mustSpill);
        set_insert(ltList, thisLt);
    }

    return thisLt;
}

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void record_variable_write(struct Set *ltList,
                           struct TACOperand *writtenOperand,
                           struct Scope *scope,
                           size_t newEnd)
{
    log(LOG_DEBUG, "Record variable write for %s at index %zu", writtenOperand->name.str, newEnd);

    u8 isGlobal = 0;
    u8 mustSpill = 0;
    struct VariableEntry *recordedVariable = writtenOperand->name.variable;
    isGlobal = recordedVariable->isGlobal;
    mustSpill = recordedVariable->mustSpill;

    // always use ->type as we don't care what it's cast as to determine its lifetime
    struct Lifetime *updatedLifetime = update_or_insert_lifetime(ltList, recordedVariable->name, tac_operand_get_type(writtenOperand), newEnd, isGlobal, mustSpill);
    updatedLifetime->nwrites += 1;
}

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void record_variable_read(struct Set *ltList,
                          struct TACOperand *readOperand,
                          struct Scope *scope,
                          size_t newEnd)
{
    log(LOG_DEBUG, "Record variable read for %s at index %zu", readOperand->name.str, newEnd);

    u8 isGlobal = 0;
    u8 mustSpill = 0;

    struct VariableEntry *recordedVariable = readOperand->name.variable;
    isGlobal = recordedVariable->isGlobal;
    mustSpill = recordedVariable->mustSpill;

    // always use ->type as we don't care what it's cast as to determine its lifetime
    struct Lifetime *updatedLifetime = update_or_insert_lifetime(ltList, recordedVariable->name, tac_operand_get_non_cast_type(readOperand), newEnd, isGlobal, mustSpill);
    updatedLifetime->nreads += 1;
}

void record_lifetime_write_for_operand(struct Set *lifetimes, struct TACOperand *operand, struct Scope *scope, size_t tacIndex)
{
    if ((tac_operand_get_type(operand)->basicType != VT_NULL) &&
        (operand->permutation != VP_LITERAL_STR) &&
        (operand->permutation != VP_LITERAL_VAL))
    {
        record_variable_write(lifetimes, operand, scope, tacIndex);
    }
}

void record_lifetime_read_for_operand(struct Set *lifetimes, struct TACOperand *operand, struct Scope *scope, size_t tacIndex)
{
    if ((tac_operand_get_type(operand)->basicType != VT_NULL) &&
        (operand->permutation != VP_LITERAL_STR) &&
        (operand->permutation != VP_LITERAL_VAL))
    {
        record_variable_read(lifetimes, operand, scope, tacIndex);
    }
}

void find_lifetimes_for_tac(struct Set *lifetimes, struct Scope *scope, struct TACLine *line, struct Stack *doDepth)
{
    // handle tt_do/tt_enddo stack and lifetime extension
    switch (line->operation)
    {
    case TT_DO:
        stack_push(doDepth, (void *)(long int)line->index);
        break;

    case TT_ENDDO:
    {
        size_t extendTo = line->index;
        size_t extendFrom = (size_t)stack_pop(doDepth);
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
        switch (get_use_of_operand(line, operandIndex))
        {
        case U_UNUSED:
            break;

        case U_READ:
            record_lifetime_read_for_operand(lifetimes, &line->operands[operandIndex], scope, line->index);
            break;

        case U_WRITE:
            record_lifetime_write_for_operand(lifetimes, &line->operands[operandIndex], scope, line->index);
            break;
        }
    }
}

void add_argument_lifetimes_for_scope(struct Set *lifetimes, struct Scope *scope)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];
        if (thisMember->type == E_ARGUMENT)
        {
            struct VariableEntry *theArgument = thisMember->entry;
            // arguments can be mustSpill too - if they are used in an address-of it will be required not to ever load them into registers
            update_or_insert_lifetime(lifetimes, thisMember->name, &theArgument->type, 0, 0, theArgument->mustSpill)->isArgument = 1;
        }
    }
}

struct Set *find_lifetimes(struct Scope *scope, struct LinkedList *basicBlockList)
{
    struct Set *lifetimes = set_new((ssize_t(*)(void *, void *))lifetime_compare, free);

    add_argument_lifetimes_for_scope(lifetimes, scope);

    struct LinkedListNode *blockRunner = basicBlockList->head;
    struct Stack *doDepth = stack_new();
    while (blockRunner != NULL)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        struct LinkedListNode *tacRunner = thisBlock->TACList->head;
        while (tacRunner != NULL)
        {
            struct TACLine *thisLine = tacRunner->data;
            find_lifetimes_for_tac(lifetimes, scope, thisLine, doDepth);
            tacRunner = tacRunner->next;
        }
        blockRunner = blockRunner->next;
    }

    stack_free(doDepth);

    return lifetimes;
}

/*
 *
 * Register struct
 *
 */
struct Register *register_new(u8 index)
{
    struct Register *reg = malloc(sizeof(struct Register));
    reg->containedLifetime = NULL;
    reg->index = index;

    return reg;
}

bool register_is_live(struct Register *reg, size_t index)
{
    if (reg->containedLifetime == NULL)
    {
        return false;
    }

    return lifetime_is_live_at_index(reg->containedLifetime, index);
}

struct MachineInfo *(*setupMachineInfo)() = NULL;

struct MachineInfo *machine_info_new(u8 maxReg,
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

void machine_info_free(struct MachineInfo *info)
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
