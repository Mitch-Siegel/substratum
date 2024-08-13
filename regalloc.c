#include "regalloc.h"

#include <string.h>

#include "codegen_generic.h"
#include "log.h"
#include "regalloc_generic.h"
#include "symtab.h"
#include "util.h"

#include "mbcl/list.h"
#include "mbcl/stack.h"

#undef set_copy

Set *set_copy_fun(Set *set)
{
    Set *copied = set_new(NULL, set->compareData);
    Iterator *setI = set_begin(set);
    while (iterator_gettable(setI))
    {
        set_insert(copied, iterator_get(setI));
        iterator_next(setI);
    }
    iterator_free(setI);
    return copied;
}

#define set_copy(set) set_copy_fun(set)

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

size_t find_max_tac_index(Set *lifetimes)
{
    size_t maxIndex = 0;
    Iterator *ltRunner = NULL;
    for (ltRunner = set_begin(lifetimes); iterator_gettable(ltRunner); iterator_next(ltRunner))
    {
        struct Lifetime *examinedLifetime = iterator_get(ltRunner);
        if (examinedLifetime->end > maxIndex)
        {
            maxIndex = examinedLifetime->end;
        }
    }
    iterator_free(ltRunner);

    return maxIndex;
}

// return an array of sets, indexed by TAC index
Array *find_lifetime_overlaps(Set *lifetimes, size_t largestTACIndex)
{
    // TODO: actual set_free function
    Array *lifetimeOverlaps = array_new((void (*)(void *))rb_tree_free, largestTACIndex + 1);
    for (size_t tacIndex = 0; tacIndex <= largestTACIndex; tacIndex++)
    {
        array_emplace(lifetimeOverlaps, tacIndex, set_new(NULL, (ssize_t(*)(void *, void *))lifetime_compare));
    }

    for (size_t overlapIndex = 0; overlapIndex <= largestTACIndex; overlapIndex++)
    {
        Iterator *ltRunner = NULL;
        for (ltRunner = set_begin(lifetimes); iterator_gettable(ltRunner); iterator_next(ltRunner))
        {
            struct Lifetime *examinedLifetime = iterator_get(ltRunner);
            if (lifetime_is_live_at_index(examinedLifetime, overlapIndex))
            {
                set_insert(array_at(lifetimeOverlaps, overlapIndex), examinedLifetime);
            }
        }
        iterator_free(ltRunner);
    }

    return lifetimeOverlaps;
}

struct Lifetime *remove_lifetime_with_best_heuristic(Set *lifetimesInContention)
{
    size_t bestHeuristic = 0;
    struct Lifetime *bestLifetime = NULL;

    Iterator *ltRunner = NULL;
    for (ltRunner = set_begin(lifetimesInContention); iterator_gettable(ltRunner); iterator_next(ltRunner))
    {
        struct Lifetime *examinedLt = iterator_get(ltRunner);
        size_t examinedHeuristic = lifetime_heuristic(examinedLt);
        if (examinedHeuristic > bestHeuristic)
        {
            bestHeuristic = examinedHeuristic;
            bestLifetime = examinedLt;
        }
    }
    iterator_free(ltRunner);

    set_remove(lifetimesInContention, bestLifetime);
    return bestLifetime;
}

void setup_local_stack(struct RegallocMetadata *metadata, struct MachineInfo *info, List *localStackLifetimes)
{
    // local offset always at least MACHINE_REGISTER_SIZE_BYTES to save frame pointer
    ssize_t localOffset = ((ssize_t)-1 * MACHINE_REGISTER_SIZE_BYTES);

    // figure out which callee-saved registers this function touches, and add space for them to the local stack offset
    Stack *touchedCalleeSaved = stack_new(NULL);
    for (size_t calleeSaveIndex = 0; calleeSaveIndex < info->callee_save.size; calleeSaveIndex++)
    {
        if (set_find(metadata->touchedRegisters, array_at(&info->callee_save, calleeSaveIndex)))
        {
            stack_push(touchedCalleeSaved, array_at(&info->callee_save, calleeSaveIndex));
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
    for (localIterator = list_begin(localStackLifetimes); iterator_gettable(localIterator); iterator_next(localIterator))
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
    for (argIterator = list_begin(argumentStackLifetimes); iterator_gettable(argIterator); iterator_next(argIterator))
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

struct LifetimePlusSize *package_lifetime_and_size(struct Lifetime *lifetime, size_t size)
{
    struct LifetimePlusSize *lts = malloc(sizeof(struct LifetimePlusSize));
    lts->lt = lifetime;
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
    Iterator *ltRunner = NULL;
    for (ltRunner = set_begin(metadata->allLifetimes); iterator_gettable(ltRunner); iterator_next(ltRunner))
    {
        struct Lifetime *examinedLt = iterator_get(ltRunner);
        if (examinedLt->wbLocation == WB_STACK)
        {
            list_append(lifetimesPlusSizes, package_lifetime_and_size(examinedLt, type_get_size(&examinedLt->type, metadata->scope)));
        }
    }
    iterator_free(ltRunner);

    list_sort(lifetimesPlusSizes);

    return lifetimesPlusSizes;
}

void allocate_stack_space(struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    List *sortedStackLifetimes = get_sorted_stack_lifetimes(metadata);

    List *localStackLifetimes = list_new(NULL, NULL);
    List *argumentStackLifetimes = list_new(NULL, NULL);
    Iterator *lifetimeIterator = list_begin(sortedStackLifetimes);
    while (iterator_gettable(lifetimeIterator))
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
    for (printIterator = list_begin(localStackLifetimes); iterator_gettable(printIterator); iterator_next(printIterator))
    {
        struct Lifetime *localLt = iterator_get(printIterator);
        log(LOG_DEBUG, "BP%zd: %s", localLt->writebackInfo.stackOffset, localLt->name);
    }
    iterator_free(printIterator);

    for (printIterator = list_begin(argumentStackLifetimes); iterator_gettable(printIterator); iterator_next(printIterator))
    {
        struct Lifetime *argLt = iterator_get(printIterator);
        log(LOG_DEBUG, "BP+%zd: %s", argLt->writebackInfo.stackOffset, argLt->name);
    }
    iterator_free(printIterator);

    list_free(localStackLifetimes);
    list_free(argumentStackLifetimes);
}

Set *pre_select_register_contention_lifetimes(Set *selectFrom, struct Scope *scope)
{
    // from the start, all lifetimes from which we are selecting are in contention

    Set *intermediate = set_copy(selectFrom);
    set_clear(selectFrom);

    Set *selectedLifetimes = set_new(NULL, selectFrom->compareData);

    // remove lifetimes which
    Iterator *ltRunner = NULL;
    for (ltRunner = set_begin(intermediate); iterator_gettable(ltRunner); iterator_next(ltRunner))
    {
        struct Lifetime *examinedLt = iterator_get(ltRunner);

        switch (examinedLt->wbLocation)
        {
            // if the lifetime already has a location, don't re-add to selectFrom as there is no more work to do for it
        case WB_GLOBAL:
        case WB_STACK:
        case WB_REGISTER:
            break;

        case WB_UNKNOWN:
            // if we are potentially going to assign a register to this lifetime, make sure it is small enough to fit in a register
            if (type_get_size(&examinedLt->type, scope) > MACHINE_REGISTER_SIZE_BYTES)
            {
                set_insert(selectFrom, examinedLt);
            }
            else
            {
                set_insert(selectedLifetimes, examinedLt);
            }
            break;
        }
    }
    iterator_free(ltRunner);
    set_free(intermediate);
    return selectedLifetimes;
}

size_t find_highest_overlap(Array *lifetimeOverlaps)
{
    size_t highestOverlap = 0;
    for (size_t tacIndex = 0; tacIndex < lifetimeOverlaps->size; tacIndex++)
    {
        size_t thisOverlap = ((Set *)array_at(lifetimeOverlaps, tacIndex))->size;
        if (thisOverlap > highestOverlap)
        {
            highestOverlap = thisOverlap;
        }
    }

    return highestOverlap;
}

void lifetime_overlaps_remove(Array *lifetimeOverlaps, struct Lifetime *toRemove)
{
    for (size_t tacIndex = 0; tacIndex < lifetimeOverlaps->size; tacIndex++)
    {
        Set *removeFrom = array_at(lifetimeOverlaps, tacIndex);
        if (set_find(removeFrom, toRemove) != NULL)
        {
            set_remove(removeFrom, toRemove);
        }
    }
}

// selectFrom: set of pointers to lifetimes which are in contention for registers
// registerPool: stack of registers (raw values in void * form) which are available to allocate
// returns: set of lifetimes which were allocated registers, leaving only lifetimes which were not given registers in selectFrom
Set *select_register_lifetimes(struct RegallocMetadata *metadata, Set *selectFrom, Stack *registerPool)
{
    Set *registerContentionLifetimes = pre_select_register_contention_lifetimes(selectFrom, metadata->function->mainScope);

    Array *lifetimeOverlaps = find_lifetime_overlaps(registerContentionLifetimes, metadata->largestTacIndex);

    // while there are too many lifetimes
    size_t nReg = registerPool->size;
    size_t deg = 0;
    while ((deg = find_highest_overlap(lifetimeOverlaps)) > nReg)
    {
        // grab the one with the best heuristic, remove it, and re-add to selectFrom
        struct Lifetime *toSpill = remove_lifetime_with_best_heuristic(registerContentionLifetimes);
        log(LOG_DEBUG, "Spill %s because max number of livetimes %zu exceeds number of available registers %zu", toSpill->name, deg, nReg);
        lifetime_overlaps_remove(lifetimeOverlaps, toSpill);
        set_insert(selectFrom, toSpill);
    }

    array_free(lifetimeOverlaps);

    Set *liveLifetimes = set_new(NULL, (ssize_t(*)(void *, void *))lifetime_compare);

    Set *needRegisters = set_copy(registerContentionLifetimes);
    needRegisters->freeData = NULL;

    // iterate by TAC index
    for (size_t tacIndex = 0; (tacIndex <= metadata->largestTacIndex) && (needRegisters->size > 0); tacIndex++)
    {
        // iterate lifetimes which are currently live
        Set *previouslyLive = set_copy(liveLifetimes);
        Iterator *liveLtRunner = NULL;
        for (liveLtRunner = set_begin(previouslyLive); iterator_gettable(liveLtRunner); iterator_next(liveLtRunner))
        {
            struct Lifetime *liveLt = iterator_get(liveLtRunner);

            // if expiring at this index, give its register back (value can still be read out of the register at tacIndex)
            if (!lifetime_is_live_after_index(liveLt, tacIndex))
            {
                set_remove(liveLifetimes, liveLt);
                stack_push(registerPool, liveLt->writebackInfo.regLocation);
                log(LOG_DEBUG, "Lifetime %s expires at %zu, freeing register %s", liveLt->name, tacIndex, liveLt->writebackInfo.regLocation->name);
            }
        }
        iterator_free(liveLtRunner);
        set_free(previouslyLive);

        // iterate lifetimes which need registers
        Iterator *newLtRunner = NULL;
        Set *previouslyNeedRegisters = set_copy(needRegisters);
        for (newLtRunner = set_begin(previouslyNeedRegisters); iterator_gettable(newLtRunner); iterator_next(newLtRunner))
        {
            struct Lifetime *examinedLt = iterator_get(newLtRunner);

            // if a lifetime becomes live at this index, assign it a register
            if (lifetime_is_live_at_index(examinedLt, tacIndex))
            {
                set_remove(needRegisters, examinedLt);
                examinedLt->writebackInfo.regLocation = (struct Register *)stack_pop(registerPool);
                examinedLt->wbLocation = WB_REGISTER;
                set_insert(liveLifetimes, examinedLt);

                set_try_insert(metadata->touchedRegisters, examinedLt->writebackInfo.regLocation);
                log(LOG_DEBUG, "Lifetime %s starts at at %zu, consuming register %s", examinedLt->name, tacIndex, examinedLt->writebackInfo.regLocation->name);
            }
        }
        set_free(previouslyNeedRegisters);
        iterator_free(newLtRunner);
    }
    set_free(needRegisters);
    set_free(liveLifetimes);

    return registerContentionLifetimes;
}

void allocate_argument_registers(struct RegallocMetadata *metadata, struct MachineInfo *machineInfo)
{
    Set *argumentLifetimes = set_new(NULL, metadata->allLifetimes->compareData);

    Iterator *ltRunner = NULL;
    for (ltRunner = set_begin(metadata->allLifetimes); iterator_gettable(ltRunner); iterator_next(ltRunner))
    {
        struct Lifetime *potentialArgumentLt = iterator_get(ltRunner);
        if (potentialArgumentLt->isArgument)
        {
            set_insert(argumentLifetimes, potentialArgumentLt);
        }
    }
    iterator_free(ltRunner);
    ltRunner = NULL;

    Stack *argumentRegisterPool = stack_new(NULL);

    Iterator *argRegI = NULL;
    // the set is traversed backwards and registers are pushed to a stack to allocate from, account for this when adding to the corresponding machineInfo register array
    for (argRegI = array_end(&machineInfo->arguments); iterator_gettable(argRegI); iterator_prev(argRegI))
    {
        stack_push(argumentRegisterPool, iterator_get(argRegI));
    }
    iterator_free(argRegI);

    set_free(select_register_lifetimes(metadata, argumentLifetimes, argumentRegisterPool));
    stack_free(argumentRegisterPool);

    // any arguments which we couldn't allocate a register for go on the stack
    for (ltRunner = set_begin(argumentLifetimes); iterator_gettable(ltRunner); iterator_next(ltRunner))
    {
        struct Lifetime *nonRegisterLifetime = iterator_get(ltRunner);
        nonRegisterLifetime->wbLocation = WB_STACK;
    }
    iterator_free(ltRunner);

    set_free(argumentLifetimes);
}

void allocate_general_registers(struct RegallocMetadata *metadata, struct MachineInfo *machineInfo)
{
    Set *registerContentionLifetimes = set_copy(metadata->allLifetimes);
    registerContentionLifetimes->freeData = NULL;

    Stack *registerPool = stack_new(NULL);

    Iterator *gpRegI = NULL;
    // the set is traversed backwards and registers are pushed to a stack to allocate from, account for this when adding to the corresponding machineInfo register array
    for (gpRegI = array_end(&machineInfo->generalPurpose); iterator_gettable(gpRegI); iterator_prev(gpRegI))
    {
        struct Register *gpReg = iterator_get(gpRegI);
        log(LOG_DEBUG, "%s is a gp reg to allocate", gpReg);
        stack_push(registerPool, gpReg);
    }
    iterator_free(gpRegI);

    set_free(select_register_lifetimes(metadata, registerContentionLifetimes, registerPool));
    stack_free(registerPool);

    // any general-purpose lifetimes which we couldn't allocate a register for go on the stack
    Iterator *ltRunner = NULL;
    for (ltRunner = set_begin(registerContentionLifetimes); iterator_gettable(ltRunner); iterator_next(ltRunner))
    {
        struct Lifetime *nonRegisterLifetime = iterator_get(ltRunner);
        log(LOG_DEBUG, "Lifetime %s wasn't assigned a register - give it a stack writeback", nonRegisterLifetime->name);
        nonRegisterLifetime->wbLocation = WB_STACK;
    }
    iterator_free(ltRunner);

    set_free(registerContentionLifetimes);
}

// really this is "figure out which lifetimes get a register"
void allocate_registers(struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    log(LOG_INFO, "Allocate registers for %s", metadata->function->name);

    // register pointers are unique and only one should exist for a given register
    metadata->touchedRegisters = set_new(NULL, register_compare);

    // assume we will always touch the stack pointer
    set_insert(metadata->touchedRegisters, info->stackPointer);

    // if we call another function we will touch the frame pointer
    if (metadata->function->callsOtherFunction)
    {
        set_insert(metadata->touchedRegisters, info->returnAddress);
        set_insert(metadata->touchedRegisters, info->framePointer);
    }

    metadata->allLifetimes = find_lifetimes(metadata->function->mainScope, metadata->function->BasicBlockList);

    metadata->largestTacIndex = find_max_tac_index(metadata->allLifetimes);

    allocate_argument_registers(metadata, info);
    allocate_general_registers(metadata, info);

    allocate_stack_space(metadata, info);

    char *ltLengthString = malloc(metadata->largestTacIndex + 3);
    Iterator *ltRunner = NULL;
    for (ltRunner = set_begin(metadata->allLifetimes); iterator_gettable(ltRunner); iterator_next(ltRunner))
    {
        const u8 LOC_STR_LEN = 16;
        char location[LOC_STR_LEN + 1];
        struct Lifetime *printedLt = iterator_get(ltRunner);
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

        log(LOG_INFO, "%40s:%s:%s", printedLt->name, location, ltLengthString);
    }
    iterator_free(ltRunner);
    free(ltLengthString);
}

void allocate_registers_for_scope(struct Scope *scope, struct MachineInfo *info);

void allocate_registers_for_struct(struct StructEntry *theStruct, struct MachineInfo *info)
{
    switch (theStruct->genericType)
    {
    case G_NONE:
        allocate_registers_for_scope(theStruct->members, info);
        break;

    case G_BASE:
    {
        Iterator *instanceIter = NULL;
        for (instanceIter = hash_table_begin(theStruct->generic.base.instances); iterator_gettable(instanceIter); iterator_next(instanceIter))
        {
            HashTableEntry *instanceEntry = iterator_get(instanceIter);
            struct StructEntry *thisInstance = instanceEntry->value;
            allocate_registers_for_struct(thisInstance, info);
        }
        iterator_free(instanceIter);
    }
    break;

    case G_INSTANCE:
        allocate_registers_for_scope(theStruct->members, info);
        break;
    }
}

void allocate_registers_for_scope(struct Scope *scope, struct MachineInfo *info)
{
    Iterator *entryIterator = NULL;
    for (entryIterator = set_begin(scope->entries); iterator_gettable(entryIterator); iterator_next(entryIterator))
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
            allocate_registers_for_struct(thisStruct, info);
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
