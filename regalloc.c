#include "regalloc.h"

#include <string.h>

#include "codegen_generic.h"
#include "log.h"
#include "regalloc_generic.h"
#include "symtab.h"
#include "util.h"

#include "mbcl/list.h"
#include "mbcl/stack.h"

// return the heuristic for how good a given lifetime is to spill - lower is better
size_t lifetime_heuristic(struct Lifetime *lifetime)
{
    // const size_t argumentSpillHeuristicMultiplier = 10;
    // base heuristic is lifetime length
    size_t heuristic = lifetime->end - lifetime->start;

    // add the number of reads and writes for this variable since they have some cost
    heuristic += lifetime->nreads;
    heuristic *= lifetime->nwrites;

    // TODO: super-prefer to "spill" arguments as they already have a stack address
    // TODO: tests that demonstrate various spilling characteristics (functions with and without arguments)

    return heuristic;
}

size_t find_max_tac_index(struct Set *lifetimes)
{
    size_t maxIndex = 0;
    for (struct LinkedListNode *ltRunner = lifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *examinedLifetime = ltRunner->data;
        if (examinedLifetime->end > maxIndex)
        {
            maxIndex = examinedLifetime->end;
        }
    }

    return maxIndex;
}

struct Set **find_lifetime_overlaps(struct Set *lifetimes, size_t largestTACIndex)
{
    struct Set **lifetimeOverlaps = malloc((largestTACIndex + 1) * sizeof(struct Set *));
    for (size_t tacIndex = 0; tacIndex <= largestTACIndex; tacIndex++)
    {
        lifetimeOverlaps[tacIndex] = old_set_new((ssize_t(*)(void *, void *))lifetime_compare, NULL);
    }

    for (size_t overlapIndex = 0; overlapIndex <= largestTACIndex; overlapIndex++)
    {
        for (struct LinkedListNode *ltRunner = lifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
        {
            struct Lifetime *examinedLifetime = ltRunner->data;
            if (lifetime_is_live_at_index(examinedLifetime, overlapIndex))
            {
                old_set_insert(lifetimeOverlaps[overlapIndex], examinedLifetime);
            }
        }
    }

    return lifetimeOverlaps;
}

struct Lifetime *remove_lifetime_with_best_heuristic(struct Set *lifetimesInContention)
{
    size_t bestHeuristic = 0;
    struct Lifetime *bestLifetime = NULL;

    for (struct LinkedListNode *ltRunner = lifetimesInContention->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *examinedLt = ltRunner->data;
        size_t examinedHeuristic = lifetime_heuristic(examinedLt);
        if (examinedHeuristic > bestHeuristic)
        {
            bestHeuristic = examinedHeuristic;
            bestLifetime = examinedLt;
        }
    }

    old_set_delete(lifetimesInContention, bestLifetime);
    return bestLifetime;
}

void setup_local_stack(struct RegallocMetadata *metadata, struct MachineInfo *info, List *localStackLifetimes)
{
    // local offset always at least MACHINE_REGISTER_SIZE_BYTES to save frame pointer
    ssize_t localOffset = ((ssize_t)-1 * MACHINE_REGISTER_SIZE_BYTES);

    // figure out which callee-saved registers this function touches, and add space for them to the local stack offset
    Stack *touchedCalleeSaved = stack_new(NULL);
    for (size_t calleeSaveIndex = 0; calleeSaveIndex < info->n_callee_save; calleeSaveIndex++)
    {
        if (old_set_find(metadata->touchedRegisters, info->callee_save[calleeSaveIndex]))
        {
            stack_push(touchedCalleeSaved, info->callee_save[calleeSaveIndex]);
        }
    }
    localOffset -= ((ssize_t)touchedCalleeSaved->size * MACHINE_REGISTER_SIZE_BYTES);
    stack_free(touchedCalleeSaved);

    if (localStackLifetimes->size == 0)
    {
        while (localOffset % STACK_ALIGN_BYTES)
        {
            localOffset--;
        }

        metadata->localStackSize = -1 * localOffset;
        return;
    }

    log(LOG_DEBUG, "Function locals for %s end at frame pointer offset %zd - %zd through 0 offset from %s are callee-saved registers", metadata->function->name, localOffset, localOffset, info->framePointer->name);

    Iterator *localIterator = NULL;
    for (localIterator = list_begin(localStackLifetimes); iterator_valid(localIterator); iterator_next(localIterator))
    {
        struct Lifetime *printedStackLt = iterator_get(localIterator);
        localOffset -= (ssize_t)type_get_size(&printedStackLt->type, metadata->function->mainScope);
        localOffset -= (ssize_t)scope_compute_padding_for_alignment(metadata->function->mainScope, &printedStackLt->type, localOffset);
        printedStackLt->writebackInfo.stackOffset = localOffset;

        log(LOG_DEBUG, "Assign stack offset %zd to lifetime %s", printedStackLt->writebackInfo.stackOffset, printedStackLt->name);
    }
    iterator_free(localIterator);

    while (localOffset % STACK_ALIGN_BYTES)
    {
        localOffset--;
    }

    metadata->localStackSize = -1 * localOffset;
}

void setup_argument_stack(struct RegallocMetadata *metadata, List *argumentStackLifetimes)
{
    if (argumentStackLifetimes->size == 0)
    {
        return;
    }

    // always save frame pointer and return address to stack
    ssize_t argOffset = (ssize_t)2 * MACHINE_REGISTER_SIZE_BYTES;
    Iterator *argIterator = NULL;
    for (argIterator = list_begin(argumentStackLifetimes); iterator_valid(argIterator); iterator_next(argIterator))
    {
        struct Lifetime *printedStackLt = iterator_get(argIterator);
        argOffset += (ssize_t)scope_compute_padding_for_alignment(metadata->function->mainScope, &printedStackLt->type, argOffset);
        printedStackLt->writebackInfo.stackOffset = argOffset;
        argOffset += (ssize_t)type_get_size(&printedStackLt->type, metadata->function->mainScope);

        log(LOG_DEBUG, "Assign stack offset %zd to argument lifetime %s", printedStackLt->writebackInfo.stackOffset, printedStackLt->name);
    }
    iterator_free(argIterator);

    while (argOffset % STACK_ALIGN_BYTES)
    {
        argOffset++;
    }

    metadata->argStackSize = argOffset;
}

struct LifetimePlusSize
{
    struct Lifetime *lt;
    size_t size;
};

struct LifetimePlusSize *package_lifetime_and_size(struct Lifetime *lt, size_t size)
{
    struct LifetimePlusSize *lts = malloc(sizeof(struct LifetimePlusSize));
    lts->lt = lt;
    lts->size = size;
    return lts;
}

static ssize_t lifetime_plus_size_compare(void *dataA, void *dataB)
{
    struct LifetimePlusSize *ltA = dataA;
    struct LifetimePlusSize *ltB = dataB;
    return (ssize_t)ltA->size - (ssize_t)ltB->size;
}

List *get_sorted_stack_lifetimes(struct RegallocMetadata *metadata)
{
    List *lifetimesPlusSizes = list_new(free, lifetime_plus_size_compare);

    // go over all lifetimes, if they have a stack writeback location we need to deal with them
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *examinedLt = ltRunner->data;
        if (examinedLt->wbLocation == WB_STACK)
        {
            list_append(lifetimesPlusSizes, package_lifetime_and_size(examinedLt, type_get_size(&examinedLt->type, metadata->scope)));
        }
    }

    list_sort(lifetimesPlusSizes);

    return lifetimesPlusSizes;
}

void allocate_stack_space(struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    List *sortedStackLifetimes = get_sorted_stack_lifetimes(metadata);

    List *localStackLifetimes = list_new(NULL, NULL);
    List *argumentStackLifetimes = list_new(NULL, NULL);
    Iterator *lifetimeIterator = list_begin(sortedStackLifetimes);
    while (iterator_valid(lifetimeIterator))
    {
        struct LifetimePlusSize *lts = iterator_get(lifetimeIterator);
        if (lts->lt->isArgument)
        {
            list_append(argumentStackLifetimes, lts->lt);
        }
        else
        {
            list_append(localStackLifetimes, lts->lt);
        }

        iterator_next(lifetimeIterator);
    }
    iterator_free(lifetimeIterator);
    list_free(sortedStackLifetimes);

    setup_local_stack(metadata, info, localStackLifetimes);
    setup_argument_stack(metadata, argumentStackLifetimes);

    Iterator *printIterator = NULL;
    for (printIterator = list_begin(localStackLifetimes); iterator_valid(printIterator); iterator_next(printIterator))
    {
        struct Lifetime *localLt = iterator_get(printIterator);
        log(LOG_DEBUG, "BP%zd: %s", localLt->writebackInfo.stackOffset, localLt->name);
    }
    iterator_free(printIterator);

    for (printIterator = list_begin(argumentStackLifetimes); iterator_valid(printIterator); iterator_next(printIterator))
    {
        struct Lifetime *argLt = iterator_get(printIterator);
        log(LOG_DEBUG, "BP+%zd: %s", argLt->writebackInfo.stackOffset, argLt->name);
    }
    iterator_free(printIterator);

    list_free(localStackLifetimes);
    list_free(argumentStackLifetimes);
}

struct Set *pre_select_register_contention_lifetimes(struct Set *selectFrom, struct Scope *scope)
{
    // from the start, all lifetimes from which we are selecting are in contention
    struct Set *contentionLifetimes = old_set_new(selectFrom->compareFunction, NULL);
    old_set_merge(contentionLifetimes, selectFrom);
    old_set_clear(selectFrom);

    // remove lifetimes which
    for (struct LinkedListNode *ltRunner = contentionLifetimes->elements->head; ltRunner != NULL;)
    {
        struct LinkedListNode *next = ltRunner->next;

        struct Lifetime *examinedLt = ltRunner->data;

        switch (examinedLt->wbLocation)
        {
            // if the lifetime already has a location, just remove it from contention, don't re-add to selectFrom as there is no more work to do for it
        case WB_GLOBAL:
        case WB_STACK:
        case WB_REGISTER:
            old_set_delete(contentionLifetimes, examinedLt);
            break;

            // if we are potentially going to assign a register to this lifetime, make sure it is small enough to fit in a register
        case WB_UNKNOWN:
            if (type_get_size(&examinedLt->type, scope) > MACHINE_REGISTER_SIZE_BYTES)
            {
                old_set_delete(contentionLifetimes, examinedLt);
                old_set_insert(selectFrom, examinedLt);
            }
            break;
        }

        ltRunner = next;
    }

    return contentionLifetimes;
}

size_t find_highest_overlap(struct Set **lifetimeOverlaps, size_t largestTacIndex)
{
    size_t highestOverlap = 0;
    for (size_t tacIndex = 0; tacIndex <= largestTacIndex; tacIndex++)
    {
        if (lifetimeOverlaps[tacIndex]->elements->size > highestOverlap)
        {
            highestOverlap = lifetimeOverlaps[tacIndex]->elements->size;
        }
    }

    return highestOverlap;
}

void lifetime_overlaps_remove(struct Set **lifetimeOverlaps, size_t largestTacIndex, struct Lifetime *toRemove)
{
    for (size_t tacIndex = 0; tacIndex <= largestTacIndex; tacIndex++)
    {
        struct Set *removeFrom = lifetimeOverlaps[tacIndex];
        if (old_set_find(removeFrom, toRemove) != NULL)
        {
            old_set_delete(removeFrom, toRemove);
        }
    }
}

// selectFrom: set of pointers to lifetimes which are in contention for registers
// registerPool: set of register indices (raw values in void * form) which are available to allocate
// returns: set of lifetimes which were allocated registers, leaving only lifetimes which were not given registers in selectFrom
struct Set *select_register_lifetimes(struct RegallocMetadata *metadata, struct Set *selectFrom, struct Set *registerPool)
{
    struct Set *registerContentionLifetimes = pre_select_register_contention_lifetimes(selectFrom, metadata->function->mainScope);

    struct Set **lifetimeOverlaps = find_lifetime_overlaps(registerContentionLifetimes, metadata->largestTacIndex);

    // while there are too many lifetimes
    size_t nReg = registerPool->elements->size;
    size_t deg = 0;
    while ((deg = find_highest_overlap(lifetimeOverlaps, metadata->largestTacIndex)) > nReg)
    {
        // grab the one with the best heuristic, remove it, and re-add to selectFrom
        struct Lifetime *toSpill = remove_lifetime_with_best_heuristic(registerContentionLifetimes);
        log(LOG_DEBUG, "Spill %s because max number of livetimes %zu exceeds number of available registers %zu", toSpill->name, deg, nReg);
        lifetime_overlaps_remove(lifetimeOverlaps, metadata->largestTacIndex, toSpill);
        old_set_insert(selectFrom, toSpill);
    }

    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        old_set_free(lifetimeOverlaps[tacIndex]);
    }
    free(lifetimeOverlaps);

    Stack *availableRegisters = stack_new(NULL);
    struct Set *liveLifetimes = old_set_new((ssize_t(*)(void *, void *))lifetime_compare, NULL);
    for (struct LinkedListNode *regRunner = registerPool->elements->head; regRunner != NULL; regRunner = regRunner->next)
    {
        stack_push(availableRegisters, regRunner->data);
    }

    struct Set *needRegisters = old_set_copy(registerContentionLifetimes);
    needRegisters->dataFreeFunction = NULL;

    // iterate by TAC index
    for (size_t tacIndex = 0; (tacIndex <= metadata->largestTacIndex) && (needRegisters->elements->size > 0); tacIndex++)
    {
        // iterate lifetimes which are currently live
        for (struct LinkedListNode *liveLtRunner = liveLifetimes->elements->head; liveLtRunner != NULL;)
        {
            struct LinkedListNode *next = liveLtRunner->next;
            struct Lifetime *liveLt = liveLtRunner->data;

            // if expiring at this index, give its register back (value can still be read out of the register at tacIndex)
            if (!lifetime_is_live_at_index(liveLt, tacIndex + 1))
            {
                old_set_delete(liveLifetimes, liveLt);
                stack_push(availableRegisters, (void *)liveLt->writebackInfo.regLocation);
                log(LOG_DEBUG, "Lifetime %s expires at %zu, freeing register %s", liveLt->name, tacIndex, liveLt->writebackInfo.regLocation->name);
            }

            liveLtRunner = next;
        }

        // iterate lifetimes which need registers
        for (struct LinkedListNode *newLtRunner = needRegisters->elements->head; newLtRunner != NULL;)
        {
            struct LinkedListNode *next = newLtRunner->next;
            struct Lifetime *examinedLt = newLtRunner->data;

            // if a lifetime becomes live at this index, assign it a register
            if (lifetime_is_live_at_index(examinedLt, tacIndex))
            {
                old_set_delete(needRegisters, examinedLt);
                examinedLt->writebackInfo.regLocation = (struct Register *)stack_pop(availableRegisters);
                examinedLt->wbLocation = WB_REGISTER;
                old_set_insert(liveLifetimes, examinedLt);

                old_set_insert(metadata->touchedRegisters, examinedLt->writebackInfo.regLocation);
                log(LOG_DEBUG, "Lifetime %s starts at at %zu, consuming register %s", examinedLt->name, tacIndex, examinedLt->writebackInfo.regLocation->name);
            }

            newLtRunner = next;
        }
    }
    stack_free(availableRegisters);
    old_set_free(needRegisters);
    old_set_free(liveLifetimes);

    return registerContentionLifetimes;
}

void allocate_argument_registers(struct RegallocMetadata *metadata, struct MachineInfo *machineInfo)
{
    struct Set *argumentLifetimes = old_set_new(metadata->allLifetimes->compareFunction, NULL);
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *potentialArgumentLt = ltRunner->data;
        if (potentialArgumentLt->isArgument)
        {
            old_set_insert(argumentLifetimes, potentialArgumentLt);
        }
    }

    struct Set *argumentRegisterPool = old_set_new(ssizet_compare, NULL);
    for (u8 argRegIndex = 0; argRegIndex < machineInfo->n_arguments; argRegIndex++)
    {
        old_set_insert(argumentRegisterPool, (void *)machineInfo->arguments[argRegIndex]);
    }

    old_set_free(select_register_lifetimes(metadata, argumentLifetimes, argumentRegisterPool));
    old_set_free(argumentRegisterPool);

    // any arguments which we couldn't allocate a register for go on the stack
    for (struct LinkedListNode *ltRunner = argumentLifetimes->elements->head; ltRunner != NULL;)
    {
        struct Lifetime *nonRegisterLifetime = ltRunner->data;
        nonRegisterLifetime->wbLocation = WB_STACK;
    }

    old_set_free(argumentLifetimes);
}

void allocate_general_registers(struct RegallocMetadata *metadata, struct MachineInfo *machineInfo)
{
    struct Set *registerContentionLifetimes = old_set_copy(metadata->allLifetimes);
    registerContentionLifetimes->dataFreeFunction = NULL;

    struct Set *registerPool = old_set_new(ssizet_compare, NULL);

    // the set is traversed head->tail and registers are pushed to a stack to allocate from, account for this when adding to the generalPurpose array
    for (u8 gpRegIndex = 0; gpRegIndex < machineInfo->n_general_purpose; gpRegIndex++)
    {
        log(LOG_DEBUG, "%s is a gp reg to allocate", machineInfo->generalPurpose[gpRegIndex]->name);
        old_set_insert(registerPool, (void *)machineInfo->generalPurpose[gpRegIndex]);
    }

    old_set_free(select_register_lifetimes(metadata, registerContentionLifetimes, registerPool));
    old_set_free(registerPool);

    // any general-purpose lifetimes which we couldn't allocate a register for go on the stack
    for (struct LinkedListNode *ltRunner = registerContentionLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *nonRegisterLifetime = ltRunner->data;
        log(LOG_DEBUG, "Lifetime %s wasn't assigned a register - give it a stack writeback", nonRegisterLifetime->name);
        nonRegisterLifetime->wbLocation = WB_STACK;
    }

    old_set_free(registerContentionLifetimes);
}

// really this is "figure out which lifetimes get a register"
void allocate_registers(struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    log(LOG_DEBUG, "Allocate registers for %s", metadata->function->name);

    // register pointers are unique and only one should exist for a given register
    metadata->touchedRegisters = old_set_new(ssizet_compare, NULL);

    // assume we will always touch the stack pointer
    old_set_insert(metadata->touchedRegisters, info->stackPointer);

    // if we call another function we will touch the frame pointer
    if (metadata->function->callsOtherFunction)
    {
        old_set_insert(metadata->touchedRegisters, info->returnAddress);
        old_set_insert(metadata->touchedRegisters, info->framePointer);
    }

    metadata->allLifetimes = find_lifetimes(metadata->function->mainScope, metadata->function->BasicBlockList);

    metadata->largestTacIndex = find_max_tac_index(metadata->allLifetimes);

    allocate_argument_registers(metadata, info);
    allocate_general_registers(metadata, info);

    allocate_stack_space(metadata, info);

    char *ltLengthString = malloc(metadata->largestTacIndex + 3);
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        const u8 LOC_STR_LEN = 16;
        char location[LOC_STR_LEN + 1];
        struct Lifetime *printedLt = ltRunner->data;
        switch (printedLt->wbLocation)
        {
        case WB_GLOBAL:
            sprintf(location, "GLOBAL");
            break;
        case WB_REGISTER:
            sprintf(location, "REG:%s", printedLt->writebackInfo.regLocation->name);
            break;
        case WB_STACK:
            sprintf(location, "STK:%zd", printedLt->writebackInfo.stackOffset);
            break;
        case WB_UNKNOWN:
            sprintf(location, "???????");
            break;
        }

        size_t len = 0;
        while (len <= printedLt->end)
        {
            if ((len < printedLt->start) || (len > printedLt->end))
            {
                len += sprintf(ltLengthString + len, " ");
            }
            else
            {
                len += sprintf(ltLengthString + len, "*");
            }
        }

        log(LOG_DEBUG, "%40s:%s:%s", printedLt->name, location, ltLengthString);
    }
    free(ltLengthString);
}

void allocate_registers_for_scope(struct Scope *scope, struct MachineInfo *info)
{
    Iterator *entryIterator = NULL;
    for (entryIterator = set_begin(scope->entries); iterator_valid(entryIterator); iterator_next(entryIterator))
    {
        struct ScopeMember *thisMember = iterator_get(entryIterator);

        switch (thisMember->type)
        {
        case E_ARGUMENT:
        case E_VARIABLE:
            break;

        case E_STRUCT:
        {
            struct StructEntry *thisStruct = thisMember->entry;
            allocate_registers_for_scope(thisStruct->members, info);
        }
        break;

        case E_FUNCTION:
        {
            struct FunctionEntry *thisFunction = thisMember->entry;
            allocate_registers(&thisFunction->regalloc, info);
        }
        break;

        case E_SCOPE:
        {
            allocate_registers_for_scope(thisMember->entry, info);
        }
        break;

        case E_BASICBLOCK:
        case E_ENUM:
            break;
        }
    }
    iterator_free(entryIterator);
}

void allocate_registers_for_program(struct SymbolTable *theTable, struct MachineInfo *info)
{
    allocate_registers_for_scope(theTable->globalScope, info);
}
