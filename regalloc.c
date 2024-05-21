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

void allocateLocalStackSpace(struct CodegenMetadata *metadata)
{
    struct Stack *stackLifetimes = Stack_New();

    // go over all lifetimes, if they have a stack writeback location we need to deal with them
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *examinedLt = ltRunner->data;
        if (examinedLt->wbLocation == wb_stack)
        {
            Stack_Push(stackLifetimes, examinedLt);
        }
    }

    metadata->nStackLocations = stackLifetimes->size;

    // early return if no stack lifetimes
    if (stackLifetimes->size == 0)
    {
        Stack_Free(stackLifetimes);
        return;
    }

    metadata->stackLayout = malloc(metadata->nStackLocations * sizeof(struct StackLocation));

    // bubble sort stack lifetimes by size - early indices in stackLifetimes->data have larger sizes
    for (size_t indexI = 0; indexI < stackLifetimes->size - 1; indexI++)
    {
        for (size_t indexJ = indexI; indexJ < stackLifetimes->size - 1; indexJ++)
        {
            struct Lifetime *lifetimeI = stackLifetimes->data[indexI];
            struct Lifetime *lifetimeJ = stackLifetimes->data[indexJ];

            size_t sizeI = Type_GetSize(&lifetimeI->type, metadata->function->mainScope);
            size_t sizeJ = Type_GetSize(&lifetimeJ->type, metadata->function->mainScope);

            if (sizeJ > sizeI)
            {
                stackLifetimes->data[indexI] = lifetimeJ;
                stackLifetimes->data[indexJ] = lifetimeI;
            }
        }
    }

    ssize_t localOffset = 0;
    for (size_t indexI = 0; indexI < stackLifetimes->size - 1; indexI++)
    {
        struct Lifetime *printedStackLt = stackLifetimes->data[indexI];
        localOffset -= Type_GetSize(&printedStackLt->type, metadata->function->mainScope);
        localOffset -= Scope_ComputePaddingForAlignment(metadata->function->mainScope, &printedStackLt->type, localOffset);
        Log(LOG_DEBUG, "%3zu @%3zd - %s", Type_GetSize(&printedStackLt->type, metadata->function->mainScope), localOffset, printedStackLt->name);
    }

    Stack_Free(stackLifetimes);
}

// return a set of lifetimes which can exist with at most availableRegisters.size simultaneously available, leaving non-selected lifetimes in selectFrom
struct Set *selectRegisterLifetimes(struct Set *selectFrom, size_t largestTacIndex, struct Set *registerPool)
{
    size_t nReg = registerPool->elements->size;
    struct Set *selected = Set_New(selectFrom->compareFunction, NULL);
    Set_Merge(selected, selectFrom);
    Set_Clear(selectFrom);

    struct Set **lifetimeOverlaps = findLifetimeOverlaps(selected, largestTacIndex);

    struct HashTable *interferenceGraph = HashTable_New((selected->elements->size / 10) + 1, (size_t(*)(void *))Lifetime_Hash, (ssize_t(*)(void *, void *))Lifetime_Compare, NULL, (void (*)(void *))Set_Free);

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

    // while there are too many lifetimes
    size_t deg = 0;
    while ((deg = InterferenceGraph_FindMaxDegree(interferenceGraph)) > nReg)
    {
        // grab the one with the best heuristic, remove it, and re-add to selectFrom
        struct Lifetime *toSpill = RemoveLifetimeWithBestHeuristic(selected);
        InterferenceGraph_Remove(interferenceGraph, toSpill);
        Set_Insert(selectFrom, toSpill);
    }

    struct Stack *availableRegisters = Stack_New();
    struct Set *liveLifetimes = Set_New((ssize_t(*)(void *, void *))Lifetime_Compare, NULL);
    for (struct LinkedListNode *regRunner = registerPool->elements->head; regRunner != NULL; regRunner = regRunner->next)
    {
        Stack_Push(availableRegisters, regRunner->data);
    }

    struct Set *needRegisters = Set_Copy(selected);

    // iterate by TAC index
    for (size_t tacIndex = 0; (tacIndex <= largestTacIndex) && (needRegisters->elements->size > 0); tacIndex++)
    {
        // iterate lifetimes which are currently live
        for (struct LinkedListNode *liveLtRunner = liveLifetimes->elements->head; liveLtRunner != NULL;)
        {
            struct LinkedListNode *next = liveLtRunner->next;
            struct Lifetime *liveLt = liveLtRunner->data;

            // if no longer live at this index, give its register back
            if (!Lifetime_IsLiveAtIndex(liveLt, tacIndex))
            {
                Set_Delete(liveLifetimes, liveLt);
                Stack_Push(availableRegisters, (void *)(size_t)liveLt->registerLocation);
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
                examinedLt->registerLocation = (u8)(size_t)Stack_Pop(availableRegisters);
                examinedLt->wbLocation = wb_register;
                Set_Insert(liveLifetimes, examinedLt);
            }

            newLtRunner = next;
        }
    }
    Stack_Free(availableRegisters);
    Set_Free(needRegisters);
    Set_Free(liveLifetimes);

    HashTable_Free(interferenceGraph);
    for (size_t overlapFreeIndex = 0; overlapFreeIndex <= largestTacIndex; overlapFreeIndex++)
    {
        Set_Free(lifetimeOverlaps[overlapFreeIndex]);
    }
    free(lifetimeOverlaps);

    return selected;
}

// really this is "figure out which lifetimes get a register"
void allocateRegisters(struct CodegenMetadata *metadata, struct MachineContext *machineContext)
{
    metadata->allLifetimes = findLifetimes(metadata->function->mainScope, metadata->function->BasicBlockList);

    metadata->largestTacIndex = findMaxTACIndex(metadata->allLifetimes);

    struct Set *registerContentionLifetimes = Set_Copy(metadata->allLifetimes);
    registerContentionLifetimes->dataFreeFunction = NULL;

    for (struct LinkedListNode *ltRunner = registerContentionLifetimes->elements->head; ltRunner != NULL;)
    {
        struct LinkedListNode *next = ltRunner->next;
        struct Lifetime *examinedLifetime = ltRunner->data;

        switch (examinedLifetime->wbLocation)
        {
        case wb_global:
        case wb_stack:
            Set_Delete(registerContentionLifetimes, examinedLifetime);
            break;

        case wb_register:
        case wb_unknown:
            break;
        }

        ltRunner = next;
    }

    struct Set *registerPool = Set_New(ssizet_compare, NULL);
    for (u8 argRegIndex = 0; argRegIndex < machineContext->n_arguments; argRegIndex++)
    {
        Set_Insert(registerPool, (void *)(size_t)machineContext->arguments[argRegIndex].index);
    }

    Set_Free(selectRegisterLifetimes(registerContentionLifetimes, metadata->largestTacIndex, registerPool));
    Set_Free(registerPool);

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
            sprintf(location, "REG:%s", registerNames[printedLt->registerLocation]);
            break;
        case wb_stack:
            sprintf(location, "STK:%zd", printedLt->stackLocation);
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

    Set_Free(registerContentionLifetimes);

    allocateLocalStackSpace(metadata);

    // for (struct LinkedListNode *interferenceRunner = metadata->allLifetimes->elements->head; interferenceRunner != NULL; interferenceRunner = interferenceRunner->next)
    // {
    //     struct Lifetime *currentInterference = interferenceRunner.
    // }
}
