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
                Log(LOG_DEBUG, "%s is live at %zu", examinedLifetime->name, overlapIndex);
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
    // struct Set *interferenceSetB = HashTable_Lookup(interferenceGraph, lifetimeB);
    if (interferenceSetA == NULL)
    {
        interferenceSetA = Set_New((ssize_t(*)(void *, void *))Lifetime_Compare, NULL);
        HashTable_Insert(interferenceGraph, lifetimeA, interferenceSetA);
    }
    // if (interferenceSetB == NULL)
    // {
    //     interferenceSetB = Set_New((ssize_t(*)(void *, void *))Lifetime_Compare, NULL);
    //     HashTable_Insert(interferenceGraph, lifetimeB, interferenceSetB);
    // }

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
            struct Lifetime *keyLt = entry->key;
            Log(LOG_INFO, "key: %s", keyLt->name);
            struct Set *valueLtSet = entry->value;
            for (struct LinkedListNode *valueRunner = valueLtSet->elements->head; valueRunner != NULL; valueRunner = valueRunner->next)
            {
                struct Lifetime *valueLt = valueRunner->data;
                Log(LOG_INFO, "\tvalue:%s", valueLt->name);
            }

            if(Set_Find(valueLtSet, toRemove) != NULL)
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

void spillToMaxRegCount(struct HashTable *interferenceGraph, struct Set *registerContentionLifetimes, size_t maxReg)
{
    Log(LOG_INFO, "Lifetimes in contention for a register:");
    for (struct LinkedListNode *contentionRunner = registerContentionLifetimes->elements->head; contentionRunner != NULL; contentionRunner = contentionRunner->next)
    {
        struct Lifetime *contentionLt = contentionRunner->data;
        Log(LOG_INFO, "\tvalue:%s", contentionLt->name);
    }

    size_t deg = 0;
    while ((deg = InterferenceGraph_FindMaxDegree(interferenceGraph)) > maxReg)
    {
        struct Lifetime *toSpill = RemoveLifetimeWithBestHeuristic(registerContentionLifetimes);
        Log(LOG_DEBUG, "Too many lifetimes - %zu - spill %s", deg, toSpill->name);
        InterferenceGraph_Remove(interferenceGraph, toSpill);
        for (size_t i = 0; i < 0xffffff; i++)
        {
        }
        toSpill->wbLocation = wb_stack;
    }
}

// really this is "figure out which lifetimes get a register"
void allocateRegisters(struct CodegenMetadata *metadata, struct MachineContext *machineContext)
{
    metadata->allLifetimes = findLifetimes(metadata->function->mainScope, metadata->function->BasicBlockList);

    metadata->largestTacIndex = findMaxTACIndex(metadata->allLifetimes);

    struct Set *registerContentionLifetimes = Set_Copy(metadata->allLifetimes);

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

    struct Set **lifetimeOverlaps = findLifetimeOverlaps(registerContentionLifetimes, metadata->largestTacIndex);

    struct HashTable *interferenceGraph = HashTable_New((registerContentionLifetimes->elements->size / 10) + 1, (size_t(*)(void *))Lifetime_Hash, (ssize_t(*)(void *, void *))Lifetime_Compare, NULL, (void (*)(void *))Set_Free);

    // for every TAC index
    for (size_t overlapIndex = 0; overlapIndex <= metadata->largestTacIndex; overlapIndex++)
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

    size_t maxReg = machineContext->n_no_save + machineContext->n_callee_saved + machineContext->n_caller_save;
    char *ltLengthString = malloc(metadata->largestTacIndex + 2);
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->elements->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *printedLt = ltRunner->data;
        i32 len = 0;
        while (len < printedLt->end)
        {
            if ((len < printedLt->start) || (len >= printedLt->end))
            {
                len += sprintf(ltLengthString + len, " ");
            }
            else
            {
                len += sprintf(ltLengthString + len, "*");
            }
        }

        Log(LOG_DEBUG, "Length: %zu - %p", len, printedLt->name);

        Log(LOG_DEBUG, "Lifetime %40s:%s", printedLt->name, ltLengthString);
    }
    spillToMaxRegCount(interferenceGraph, registerContentionLifetimes, maxReg);


    for (size_t overlapFreeIndex = 0; overlapFreeIndex < metadata->largestTacIndex; overlapFreeIndex++)
    {
        Set_Free(lifetimeOverlaps[overlapFreeIndex]);
    }
    free(lifetimeOverlaps);
    // for (struct LinkedListNode *interferenceRunner = metadata->allLifetimes->elements->head; interferenceRunner != NULL; interferenceRunner = interferenceRunner->next)
    // {
    //     struct Lifetime *currentInterference = interferenceRunner.
    // }
}
