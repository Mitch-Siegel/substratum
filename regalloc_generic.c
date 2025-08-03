#include "regalloc_generic.h"

#include "log.h"
#include "symtab.h"
#include "tac.h"
#include "util.h"
#include <mbcl/stack.h>
#include <string.h>

struct Lifetime *lifetime_find_by_name(Set *allLifetimes, char *lifetimeName)
{
    struct Lifetime dummy = {0};
    dummy.name = lifetimeName;
    return set_find(allLifetimes, &dummy);
}

struct Lifetime *lifetime_find(Set *allLifetimes, struct TACOperand *operand)
{
    if (operand->permutation == VP_LITERAL_STR || operand->permutation == VP_LITERAL_VAL)
    {
        return NULL;
    }

    return lifetime_find_by_name(allLifetimes, operand->name.variable->name);
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

// whether or not the lifetime is live at the given index
bool lifetime_is_live_at_index(struct Lifetime *lifetime, size_t index)
{
    return ((lifetime->start <= index) && (lifetime->end >= index));
}

// whether or not the lifetime is live after the end the given index
bool lifetime_is_live_after_index(struct Lifetime *lifetime, size_t index)
{
    return (lifetime->start <= index) && (lifetime->end > index);
}

// search through the list of existing lifetimes
// update the lifetime if it exists, insert if it doesn't
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *update_or_insert_lifetime(Set *lifetimes,
                                           char *name,
                                           struct Type *type,
                                           size_t newEnd,
                                           u8 isGlobal,
                                           u8 mustSpill)
{
    struct Lifetime *thisLt = lifetime_find_by_name(lifetimes, name);
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
        char *typeName = type_get_name(type);
        log(LOG_DEBUG, "Create lifetime starting at %zu for %s %s: global? %d mustspill? %d", newEnd, typeName, name, isGlobal, mustSpill);
        free(typeName);
        thisLt = lifetime_new(name, type, newEnd, isGlobal, mustSpill);
        set_insert(lifetimes, thisLt);
    }

    return thisLt;
}

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void record_variable_write(Set *lifetimes,
                           struct TACOperand *writtenOperand,
                           struct Scope *scope,
                           size_t newEnd)
{
    log(LOG_DEBUG, "Record variable write for %s at index %zu", writtenOperand->name.variable->name, newEnd);

    u8 isGlobal = 0;
    u8 mustSpill = 0;
    struct VariableEntry *recordedVariable = writtenOperand->name.variable;
    isGlobal = recordedVariable->isGlobal;
    mustSpill = recordedVariable->mustSpill;

    // always use ->type as we don't care what it's cast as to determine its lifetime
    struct Lifetime *updatedLifetime = update_or_insert_lifetime(lifetimes, recordedVariable->name, tac_operand_get_non_cast_type(writtenOperand), newEnd, isGlobal, mustSpill);
    updatedLifetime->nwrites += 1;
}

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void record_variable_read(Set *lifetimes,
                          struct TACOperand *readOperand,
                          struct Scope *scope,
                          size_t newEnd)
{
    log(LOG_DEBUG, "Record variable read for %s at index %zu", readOperand->name.variable->name, newEnd);

    u8 isGlobal = 0;
    u8 mustSpill = 0;

    struct VariableEntry *recordedVariable = readOperand->name.variable;
    isGlobal = recordedVariable->isGlobal;
    mustSpill = recordedVariable->mustSpill;

    // always use ->type as we don't care what it's cast as to determine its lifetime
    struct Lifetime *updatedLifetime = update_or_insert_lifetime(lifetimes, recordedVariable->name, tac_operand_get_non_cast_type(readOperand), newEnd, isGlobal, mustSpill);
    updatedLifetime->nreads += 1;
}

void record_lifetime_write_for_operand(Set *lifetimes, struct TACOperand *operand, struct Scope *scope, size_t tacIndex)
{
    if ((tac_operand_get_type(operand)->basicType != VT_NULL) &&
        (operand->permutation != VP_LITERAL_STR) &&
        (operand->permutation != VP_LITERAL_VAL))
    {
        record_variable_write(lifetimes, operand, scope, tacIndex);
    }
}

void record_lifetime_read_for_operand(Set *lifetimes, struct TACOperand *operand, struct Scope *scope, size_t tacIndex)
{
    if ((tac_operand_get_type(operand)->basicType != VT_NULL) &&
        (operand->permutation != VP_LITERAL_STR) &&
        (operand->permutation != VP_LITERAL_VAL))
    {
        record_variable_read(lifetimes, operand, scope, tacIndex);
    }
}

void find_lifetimes_for_tac(Set *lifetimes, struct Scope *scope, struct TACLine *line, Stack *doDepth)
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

        Iterator *lifetimeRunner = NULL;
        for (lifetimeRunner = set_begin(lifetimes); iterator_gettable(lifetimeRunner); iterator_next(lifetimeRunner))
        {
            struct Lifetime *examinedLifetime = iterator_get(lifetimeRunner);
            if (examinedLifetime->end >= extendFrom && examinedLifetime->end < extendTo)
            {
                if (examinedLifetime->name[0] != '.')
                {
                    examinedLifetime->end = extendTo + 1;
                }
            }
        }
        iterator_free(lifetimeRunner);
    }
    break;

    default:
        break;
    }

    struct OperandUsages lineUsages = get_operand_usages(line);
    while (lineUsages.reads->size > 0)
    {
        struct TACOperand *readOperand = deque_pop_front(lineUsages.reads);
        record_lifetime_read_for_operand(lifetimes, readOperand, scope, line->index);
    }

    while (lineUsages.writes->size > 0)
    {
        struct TACOperand *writeOperand = deque_pop_front(lineUsages.writes);
        record_lifetime_write_for_operand(lifetimes, writeOperand, scope, line->index);
    }

    deque_free(lineUsages.reads);
    deque_free(lineUsages.writes);
}

void add_argument_lifetimes_for_scope(Set *lifetimes, struct Scope *scope)
{
    Iterator *entryIterator = NULL;
    for (entryIterator = set_begin(scope->entries); iterator_gettable(entryIterator); iterator_next(entryIterator))
    {
        struct ScopeMember *thisMember = iterator_get(entryIterator);
        if (thisMember->type == E_ARGUMENT)
        {
            struct VariableEntry *theArgument = thisMember->entry;
            // arguments can be mustSpill too - if they are used in an address-of it will be required not to ever load them into registers
            update_or_insert_lifetime(lifetimes, thisMember->name, &theArgument->type, 0, 0, theArgument->mustSpill)->isArgument = 1;
        }
    }
    iterator_free(entryIterator);
}

Set *find_lifetimes(struct Scope *scope, List *basicBlockList)
{
    Set *lifetimes = set_new(free, (ssize_t(*)(void *, void *))lifetime_compare);

    add_argument_lifetimes_for_scope(lifetimes, scope);

    Stack *doDepth = stack_new(NULL);
    Iterator *blockRunner = NULL;
    for (blockRunner = list_begin(basicBlockList); iterator_gettable(blockRunner); iterator_next(blockRunner))
    {
        struct BasicBlock *thisBlock = iterator_get(blockRunner);
        Iterator *tacRunner = NULL;
        for (tacRunner = list_begin(thisBlock->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
        {
            struct TACLine *thisLine = iterator_get(tacRunner);
            find_lifetimes_for_tac(lifetimes, scope, thisLine, doDepth);
        }
        iterator_free(tacRunner);
    }
    iterator_free(blockRunner);

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

ssize_t register_compare(void *dataA, void *dataB)
{
    struct Register *registerA = dataA;
    struct Register *registerb = dataB;
    return registerA->index - registerb->index;
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

    array_init(&wip->allRegisters, NULL, maxReg);

    array_init(&wip->temps, NULL, n_temps);
    array_init(&wip->tempsOccupied, NULL, n_temps);

    array_init(&wip->arguments, NULL, n_arguments);

    array_init(&wip->generalPurpose, NULL, n_general_purpose);

    array_init(&wip->no_save, NULL, n_no_save);

    array_init(&wip->callee_save, NULL, n_callee_save);

    array_init(&wip->caller_save, NULL, n_caller_save);

    return wip;
}

void machine_info_free(struct MachineInfo *info)
{
    array_deinit(&info->allRegisters);
    array_deinit(&info->temps);
    array_deinit(&info->tempsOccupied);
    array_deinit(&info->generalPurpose);
    array_deinit(&info->arguments);
    array_deinit(&info->no_save);
    array_deinit(&info->callee_save);
    array_deinit(&info->caller_save);

    free(info);
}

struct Register *find_register_by_name(struct MachineInfo *info, char *name)
{
    for (size_t regIndex = 0; regIndex < info->allRegisters.size; regIndex++)
    {
        struct Register *regAtIdx = array_at(&info->allRegisters, regIndex);
        if (strcmp(regAtIdx->name, name) == 0)
        {
            return regAtIdx;
        }
    }

    return NULL;
}
