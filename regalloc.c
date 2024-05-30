#include "regalloc.h"

#include <string.h>

#include "codegen_generic.h"
#include "log.h"
#include "regalloc_generic.h"
#include "symtab.h"
#include "util.h"

// return the heuristic for how good a given lifetime is to spill - lower is better
size_t Lifetime_Heuristic(struct Lifetime *lifetime)
{
    // const size_t argumentSpillHeuristicMultiplier = 10;
    // base heuristic is lifetime length
    size_t heuristic = lifetime->end - lifetime->start;

    // add the number of reads and writes for this variable since they have some cost
    heuristic += lifetime->nreads;
    heuristic *= lifetime->nwrites;

    // TODO: super-prefer to "spill" arguments as they already have a stack address

    return heuristic;
}

size_t findMaxTACIndex(struct Set *lifetimes)
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

struct Set **findLifetimeOverlaps(struct Set *lifetimes, size_t largestTACIndex)
{
    struct Set **lifetimeOverlaps = malloc((largestTACIndex + 1) * sizeof(struct Set *));
    for (size_t TACIndex = 0; TACIndex <= largestTACIndex; TACIndex++)
    {
        lifetimeOverlaps[TACIndex] = Set_New((ssize_t(*)(void *, void *))Lifetime_Compare, NULL);
    }

    for (size_t overlapIndex = 0; overlapIndex <= largestTACIndex; overlapIndex++)
    {
        for (struct LinkedListNode *ltRunner = lifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
        {
            struct Lifetime *examinedLifetime = ltRunner->data;
            if (Lifetime_IsLiveAtIndex(examinedLifetime, overlapIndex))
            {
                Set_Insert(lifetimeOverlaps[overlapIndex], examinedLifetime);
            }
        }
    }

    return lifetimeOverlaps;
}

void InterferenceGraph_Insert(struct HashTable *interferenceGraph, struct Lifetime *lifetimeA, struct Lifetime *lifetimeB)
{
    // get the interference sets from the graph (or allocate & insert if nonexistent)
    struct Set *interferenceSetA = HashTable_Lookup(interferenceGraph, lifetimeA);
    if (interferenceSetA == NULL)
    {
        interferenceSetA = Set_New((ssize_t(*)(void *, void *))Lifetime_Compare, NULL);
        HashTable_Insert(interferenceGraph, lifetimeA, interferenceSetA);
    }

    // since A and B are live at the same time, insert them in each other's interference sets
    Set_Insert(interferenceSetA, lifetimeB);
    // Set_Insert(interferenceSetB, lifetimeA);
}

void InterferenceGraph_Remove(struct HashTable *interferenceGraph, struct Lifetime *toRemove)
{
    struct Set *interferenceSet = HashTable_Lookup(interferenceGraph, toRemove);

    for (size_t bucketIndex = 0; bucketIndex < interferenceGraph->nBuckets; bucketIndex++)
    {
        struct Set *bucket = interferenceGraph->buckets[bucketIndex];
        for (struct LinkedListNode *entryRunner = bucket->elements->head; entryRunner != NULL; entryRunner = entryRunner->next)
        {
            struct HashTableEntry *entry = entryRunner->data;
            struct Set *valueLtSet = entry->value;

            if (Set_Find(valueLtSet, toRemove) != NULL)
            {
                Set_Delete(valueLtSet, toRemove);
            }
        }
    }

    if (interferenceSet != NULL)
    {
        HashTable_Delete(interferenceGraph, toRemove);
    }
}

size_t InterferenceGraph_FindMaxDegree(struct HashTable *interferenceGraph)
{
    size_t maxDegree = 0;

    for (size_t bucketIndex = 0; bucketIndex < interferenceGraph->nBuckets; bucketIndex++)
    {
        struct Set *bucket = interferenceGraph->buckets[bucketIndex];
        for (struct LinkedListNode *ifSetRunner = bucket->elements->head; ifSetRunner != NULL; ifSetRunner = ifSetRunner->next)
        {
            struct HashTableEntry *ifGraphEntry = ifSetRunner->data;
            struct Set *ifSet = ifGraphEntry->value;
            if (ifSet->elements->size > maxDegree)
            {
                maxDegree = ifSet->elements->size;
            }
        }
    }

    return maxDegree;
}

size_t InterferenceGraph_FindMinDegree(struct HashTable *interferenceGraph)
{
    size_t minDegree = SIZE_MAX;

    for (size_t bucketIndex = 0; bucketIndex < interferenceGraph->nBuckets; bucketIndex++)
    {
        struct Set *bucket = interferenceGraph->buckets[bucketIndex];
        for (struct LinkedListNode *ifSetRunner = bucket->elements->head; ifSetRunner != NULL; ifSetRunner = ifSetRunner->next)
        {
            struct HashTableEntry *ifGraphEntry = ifSetRunner->data;
            struct Set *ifSet = ifGraphEntry->value;
            if (ifSet->elements->size < minDegree)
            {
                minDegree = ifSet->elements->size;
            }
        }
    }

    return minDegree;
}

struct Lifetime *RemoveLifetimeWithBestHeuristic(struct Set *lifetimesInContention)
{
    size_t bestHeuristic = 0;
    struct Lifetime *bestLifetime = NULL;

    for (struct LinkedListNode *ltRunner = lifetimesInContention->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *examinedLt = ltRunner->data;
        size_t examinedHeuristic = Lifetime_Heuristic(examinedLt);
        if (examinedHeuristic > bestHeuristic)
        {
            bestHeuristic = examinedHeuristic;
            bestLifetime = examinedLt;
        }
    }

    Set_Delete(lifetimesInContention, bestLifetime);
    return bestLifetime;
}

void bubbleSortLifetimesBySize(struct Stack *lifetimeStack, struct Scope *scope)
{
    // bubble sort stack lifetimes by size - early indices in stackLifetimes->data have larger sizes
    for (size_t indexI = 0; indexI < lifetimeStack->size - 1; indexI++)
    {
        for (size_t indexJ = indexI; indexJ < lifetimeStack->size - 1; indexJ++)
        {
            struct Lifetime *lifetimeI = lifetimeStack->data[indexI];
            struct Lifetime *lifetimeJ = lifetimeStack->data[indexJ];

            size_t sizeI = Type_GetSize(&lifetimeI->type, scope);
            size_t sizeJ = Type_GetSize(&lifetimeJ->type, scope);

            if (sizeJ > sizeI)
            {
                lifetimeStack->data[indexI] = lifetimeJ;
                lifetimeStack->data[indexJ] = lifetimeI;
            }
        }
    }
}

void setupLocalStack(struct RegallocMetadata *metadata, struct MachineInfo *info, struct Stack *localStackLifetimes)
{
    // local offset always at least MACHINE_REGISTER_SIZE_BYTES to save frame pointer
    size_t localOffset = (-1 * MACHINE_REGISTER_SIZE_BYTES);

    // figure out which callee-saved registers this function touches, and add space for them to the local stack offset
    struct Stack *touchedCalleeSaved = Stack_New();
    for (size_t calleeSaveIndex = 0; calleeSaveIndex < info->n_callee_save; calleeSaveIndex++)
    {
        if (Set_Find(metadata->touchedRegisters, info->callee_save[calleeSaveIndex]))
        {
            Stack_Push(touchedCalleeSaved, info->callee_save[calleeSaveIndex]);
        }
    }
    localOffset -= (touchedCalleeSaved->size * MACHINE_REGISTER_SIZE_BYTES);
    Stack_Free(touchedCalleeSaved);
    
    if (localStackLifetimes->size == 0)
    {
        while (localOffset % STACK_ALIGN_BYTES)
        {
            localOffset--;
        }

        metadata->localStackSize = -1 * localOffset;
        return;
    }

    bubbleSortLifetimesBySize(localStackLifetimes, metadata->function->mainScope);


    Log(LOG_DEBUG, "Function locals for %s end at frame pointer offset %zd - %zd through 0 offset from %s are callee-saved registers", metadata->function->name, localOffset, localOffset, info->framePointer->name);


    for (size_t indexI = 0; indexI < localStackLifetimes->size; indexI++)
    {
        struct Lifetime *printedStackLt = localStackLifetimes->data[indexI];
        localOffset -= (ssize_t)Type_GetSize(&printedStackLt->type, metadata->function->mainScope);
        localOffset -= (ssize_t)Scope_ComputePaddingForAlignment(metadata->function->mainScope, &printedStackLt->type, localOffset);
        printedStackLt->writebackInfo.stackOffset = localOffset;

        Log(LOG_DEBUG, "Assign stack offset %zd to lifetime %s", printedStackLt->writebackInfo.stackOffset, printedStackLt->name);
    }

    while (localOffset % STACK_ALIGN_BYTES)
    {
        localOffset--;
    }

    metadata->localStackSize = -1 * localOffset;
}

void setupArgumentStack(struct RegallocMetadata *metadata, struct Stack *argumentStackLifetimes)
{
    if (argumentStackLifetimes->size == 0)
    {
        return;
    }

    bubbleSortLifetimesBySize(argumentStackLifetimes, metadata->function->mainScope);

    // always save frame pointer and return address to stack
    ssize_t argOffset = (ssize_t)2 * MACHINE_REGISTER_SIZE_BYTES;
    for (size_t indexI = 0; indexI < argumentStackLifetimes->size; indexI++)
    {
        struct Lifetime *printedStackLt = argumentStackLifetimes->data[indexI];
        argOffset += (ssize_t)Scope_ComputePaddingForAlignment(metadata->function->mainScope, &printedStackLt->type, argOffset);
        printedStackLt->writebackInfo.stackOffset = argOffset;
        argOffset += (ssize_t)Type_GetSize(&printedStackLt->type, metadata->function->mainScope);

        Log(LOG_DEBUG, "Assign stack offset %zd to argument lifetime %s", printedStackLt->writebackInfo.stackOffset, printedStackLt->name);
    }

    while (argOffset % STACK_ALIGN_BYTES)
    {
        argOffset++;
    }
}

void allocateStackSpace(struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    struct Stack *localStackLifetimes = Stack_New();
    struct Stack *argumentStackLifetimes = Stack_New();

    // go over all lifetimes, if they have a stack writeback location we need to deal with them
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *examinedLt = ltRunner->data;
        if (examinedLt->wbLocation == wb_stack)
        {
            if (examinedLt->isArgument)
            {
                Stack_Push(argumentStackLifetimes, examinedLt);
            }
            else
            {
                Stack_Push(localStackLifetimes, examinedLt);
            }
        }
    }

    setupLocalStack(metadata, info, localStackLifetimes);
    setupArgumentStack(metadata, argumentStackLifetimes);

    for (size_t localI = 0; localI < localStackLifetimes->size; localI++)
    {
        struct Lifetime *localLt = localStackLifetimes->data[localStackLifetimes->size - localI - 1];
        Log(LOG_DEBUG, "BP%zd: %s", localLt->writebackInfo.stackOffset, localLt->name);
    }

    for (size_t argI = 0; argI < argumentStackLifetimes->size; argI++)
    {
        struct Lifetime *stackLt = argumentStackLifetimes->data[argI];
        Log(LOG_DEBUG, "BP+%zd: %s", stackLt->writebackInfo.stackOffset, stackLt->name);
    }

    Stack_Free(localStackLifetimes);
    Stack_Free(argumentStackLifetimes);
}

struct Set *preSelectRegisterContentionLifetimes(struct Set *selectFrom, struct Scope *scope)
{
    // from the start, all lifetimes from which we are selecting are in contention
    struct Set *contentionLifetimes = Set_New(selectFrom->compareFunction, NULL);
    Set_Merge(contentionLifetimes, selectFrom);
    Set_Clear(selectFrom);

    // remove lifetimes which
    for (struct LinkedListNode *ltRunner = contentionLifetimes->elements->head; ltRunner != NULL;)
    {
        struct LinkedListNode *next = ltRunner->next;

        struct Lifetime *examinedLt = ltRunner->data;

        switch (examinedLt->wbLocation)
        {
            // if the lifetime already has a location, just remove it from contention, don't re-add to selectFrom as there is no more work to do for it
        case wb_global:
        case wb_stack:
        case wb_register:
            Set_Delete(contentionLifetimes, examinedLt);
            break;

            // if we are potentially going to assign a register to this lifetime, make sure it is small enough to fit in a register
        case wb_unknown:
            if (Type_GetSize(&examinedLt->type, scope) > MACHINE_REGISTER_SIZE_BYTES)
            {
                Set_Delete(contentionLifetimes, examinedLt);
                Set_Insert(selectFrom, examinedLt);
            }
            break;
        }

        ltRunner = next;
    }

    return contentionLifetimes;
}

struct HashTable *generateInterferenceGraph(struct Set *registerContentionLifetimes, size_t largestTacIndex)
{
    // calculate overlaps and interference graph
    struct Set **lifetimeOverlaps = findLifetimeOverlaps(registerContentionLifetimes, largestTacIndex);
    struct HashTable *interferenceGraph = HashTable_New((registerContentionLifetimes->elements->size / 10) + 1, (size_t(*)(void *))Lifetime_Hash, (ssize_t(*)(void *, void *))Lifetime_Compare, NULL, (void (*)(void *))Set_Free);

    // for every TAC index
    for (size_t overlapIndex = 0; overlapIndex <= largestTacIndex; overlapIndex++)
    {
        // examine the overlaps at this index
        struct Set *overlapsAtIndex = lifetimeOverlaps[overlapIndex];
        for (struct LinkedListNode *overlapRunnerI = overlapsAtIndex->elements->head; overlapRunnerI != NULL; overlapRunnerI = overlapRunnerI->next)
        {
            for (struct LinkedListNode *overlapRunnerJ = overlapRunnerI->next; overlapRunnerJ != NULL; overlapRunnerJ = overlapRunnerJ->next)
            {
                // lifetimes I and J overlap with each other
                struct Lifetime *lifetimeI = overlapRunnerI->data;
                struct Lifetime *lifetimeJ = overlapRunnerJ->data;

                InterferenceGraph_Insert(interferenceGraph, lifetimeI, lifetimeJ);
            }
        }
    }

    for (size_t overlapFreeIndex = 0; overlapFreeIndex <= largestTacIndex; overlapFreeIndex++)
    {
        Set_Free(lifetimeOverlaps[overlapFreeIndex]);
    }
    free(lifetimeOverlaps);

    return interferenceGraph;
}

// selectFrom: set of pointers to lifetimes which are in contention for registers
// registerPool: set of register indices (raw values in void * form) which are available to allocate
// returns: set of lifetimes which were allocated registers, leaving only lifetimes which were not given registers in selectFrom
struct Set *selectRegisterLifetimes(struct RegallocMetadata *metadata, struct Set *selectFrom, struct Set *registerPool)
{
    struct Set *registerContentionLifetimes = preSelectRegisterContentionLifetimes(selectFrom, metadata->function->mainScope);

    struct HashTable *interferenceGraph = generateInterferenceGraph(registerContentionLifetimes, metadata->largestTacIndex);

    // while there are too many lifetimes
    size_t nReg = registerPool->elements->size;
    size_t deg = 0;
    while ((deg = InterferenceGraph_FindMaxDegree(interferenceGraph)) > nReg)
    {
        // grab the one with the best heuristic, remove it, and re-add to selectFrom
        struct Lifetime *toSpill = RemoveLifetimeWithBestHeuristic(registerContentionLifetimes);
        InterferenceGraph_Remove(interferenceGraph, toSpill);
        Set_Insert(selectFrom, toSpill);
    }

    struct Stack *availableRegisters = Stack_New();
    struct Set *liveLifetimes = Set_New((ssize_t(*)(void *, void *))Lifetime_Compare, NULL);
    for (struct LinkedListNode *regRunner = registerPool->elements->head; regRunner != NULL; regRunner = regRunner->next)
    {
        Stack_Push(availableRegisters, regRunner->data);
    }

    struct Set *needRegisters = Set_Copy(registerContentionLifetimes);
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
            if (!Lifetime_IsLiveAtIndex(liveLt, tacIndex + 1))
            {
                Set_Delete(liveLifetimes, liveLt);
                Stack_Push(availableRegisters, (void *)liveLt->writebackInfo.regLocation);
                Log(LOG_DEBUG, "Lifetime %s expires at %zu, freeing register %s", liveLt->name, tacIndex, liveLt->writebackInfo.regLocation->name);
            }

            liveLtRunner = next;
        }

        // iterate lifetimes which need registers
        for (struct LinkedListNode *newLtRunner = needRegisters->elements->head; newLtRunner != NULL;)
        {
            struct LinkedListNode *next = newLtRunner->next;
            struct Lifetime *examinedLt = newLtRunner->data;

            // if a lifetime becomes live at this index, assign it a register
            if (Lifetime_IsLiveAtIndex(examinedLt, tacIndex))
            {
                Set_Delete(needRegisters, examinedLt);
                examinedLt->writebackInfo.regLocation = (struct Register *)Stack_Pop(availableRegisters);
                examinedLt->wbLocation = wb_register;
                Set_Insert(liveLifetimes, examinedLt);

                Set_Insert(metadata->touchedRegisters, examinedLt->writebackInfo.regLocation);
                Log(LOG_DEBUG, "Lifetime %s starts at at %zu, consuming register %s", examinedLt->name, tacIndex, examinedLt->writebackInfo.regLocation->name);
            }

            newLtRunner = next;
        }
    }
    Stack_Free(availableRegisters);
    Set_Free(needRegisters);
    Set_Free(liveLifetimes);

    HashTable_Free(interferenceGraph);

    return registerContentionLifetimes;
}

void allocateArgumentRegisters(struct RegallocMetadata *metadata, struct MachineInfo *machineInfo)
{
    struct Set *argumentLifetimes = Set_New(metadata->allLifetimes->compareFunction, NULL);
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *potentialArgumentLt = ltRunner->data;
        if (potentialArgumentLt->isArgument)
        {
            Set_Insert(argumentLifetimes, potentialArgumentLt);
        }
    }

    struct Set *argumentRegisterPool = Set_New(ssizet_compare, NULL);
    for (u8 argRegIndex = 0; argRegIndex < machineInfo->n_arguments; argRegIndex++)
    {
        Set_Insert(argumentRegisterPool, (void *)machineInfo->arguments[argRegIndex]);
    }

    Set_Free(selectRegisterLifetimes(metadata, argumentLifetimes, argumentRegisterPool));
    Set_Free(argumentRegisterPool);

    // any arguments which we couldn't allocate a register for go on the stack
    for (struct LinkedListNode *ltRunner = argumentLifetimes->elements->head; ltRunner != NULL;)
    {
        struct Lifetime *nonRegisterLifetime = ltRunner->data;
        nonRegisterLifetime->wbLocation = wb_stack;
    }

    Set_Free(argumentLifetimes);
}

void allocateGeneralRegisters(struct RegallocMetadata *metadata, struct MachineInfo *machineInfo)
{
    struct Set *registerContentionLifetimes = Set_Copy(metadata->allLifetimes);
    registerContentionLifetimes->dataFreeFunction = NULL;

    struct Set *registerPool = Set_New(ssizet_compare, NULL);

    // the set is traversed head->tail and registers are pushed to a stack to allocate from. put caller-save registers first so they are at the bottom of the stack
    for (u8 gpRegIndex = 0; gpRegIndex < machineInfo->n_caller_save; gpRegIndex++)
    {
        Set_Insert(registerPool, (void *)machineInfo->caller_save[gpRegIndex]);
    }

    // callee_save at the top of the stack so they are allocated from first
    for (u8 gpRegIndex = 0; gpRegIndex < machineInfo->n_callee_save; gpRegIndex++)
    {
        Set_Insert(registerPool, (void *)machineInfo->callee_save[gpRegIndex]);
    }

    Set_Free(selectRegisterLifetimes(metadata, registerContentionLifetimes, registerPool));
    Set_Free(registerPool);

    // any general-purpose lifetimes which we couldn't allocate a register for go on the stack
    for (struct LinkedListNode *ltRunner = registerContentionLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *nonRegisterLifetime = ltRunner->data;
        nonRegisterLifetime->wbLocation = wb_stack;
    }

    Set_Free(registerContentionLifetimes);
}

// really this is "figure out which lifetimes get a register"
void allocateRegisters(struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    Log(LOG_DEBUG, "Allocate registers for %s", metadata->function->name);

    // register pointers are unique and only one should exist for a given register
    metadata->touchedRegisters = Set_New(ssizet_compare, NULL);

    // assume we will always touch the stack pointer
    Set_Insert(metadata->touchedRegisters, info->stackPointer);

    // if we call another function we will touch the frame pointer
    if (metadata->function->callsOtherFunction)
    {
        Set_Insert(metadata->touchedRegisters, info->returnAddress);
        Set_Insert(metadata->touchedRegisters, info->framePointer);
    }

    metadata->allLifetimes = findLifetimes(metadata->function->mainScope, metadata->function->BasicBlockList);

    metadata->largestTacIndex = findMaxTACIndex(metadata->allLifetimes);

    allocateArgumentRegisters(metadata, info);
    allocateGeneralRegisters(metadata, info);

    allocateStackSpace(metadata, info);

    char *ltLengthString = malloc(metadata->largestTacIndex + 3);
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        char location[16];
        struct Lifetime *printedLt = ltRunner->data;
        switch (printedLt->wbLocation)
        {
        case wb_global:
            sprintf(location, "GLOBAL");
            break;
        case wb_register:
            sprintf(location, "REG:%s", printedLt->writebackInfo.regLocation->name);
            break;
        case wb_stack:
            sprintf(location, "STK:%zd", printedLt->writebackInfo.stackOffset);
            break;
        case wb_unknown:
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

        Log(LOG_DEBUG, "%40s:%s:%s", printedLt->name, location, ltLengthString);
    }
    free(ltLengthString);
}

void allocateRegistersForScope(struct Scope *scope, struct MachineInfo *info)
{
    for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = scope->entries->data[entryIndex];

        switch (thisMember->type)
        {
        case e_argument:
        case e_variable:
            break;

        case e_struct:
        {
            struct StructEntry *thisStruct = thisMember->entry;
            allocateRegistersForScope(thisStruct->members, info);
        }
        break;

        case e_function:
        {
            struct FunctionEntry *thisFunction = thisMember->entry;
            allocateRegisters(&thisFunction->regalloc, info);
        }
        break;

        case e_scope:
        {
            allocateRegistersForScope(thisMember->entry, info);
        }
        break;

        case e_basicblock:
            break;
        }
    }
}

void allocateRegistersForProgram(struct SymbolTable *theTable, struct MachineInfo *info)
{
    allocateRegistersForScope(theTable->globalScope, info);
}
