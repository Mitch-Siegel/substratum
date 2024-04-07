#include "regalloc.h"

#include <string.h>

#include "regalloc_generic.h"
#include "codegen_generic.h"
#include "util.h"
#include "symtab.h"

// return the heuristic for how good a given lifetime is to spill - lower is better
i32 lifetimeHeuristic(struct Lifetime *lifetime)
{
    const i32 argumentSpillHeuristicMultiplier = 10;
    // base heuristic is lifetime length
    i32 heuristic = lifetime->end - lifetime->start;
    // add the number of reads for this variable since they have some cost
    heuristic += lifetime->nreads;
    // multiply by number of writes for this variable since that is a high-cost operation
    heuristic *= lifetime->nwrites;

    // inflate heuristics for cases which have no actual stack space cost to spill:
    // super-prefer to "spill" arguments as they already have a stack address
    if (!lifetime->isArgument)
    {
        heuristic *= argumentSpillHeuristicMultiplier;
    }

    return heuristic;
}

void selectRegisterVariables(struct CodegenMetadata *metadata, size_t mostConcurrentLifetimes)
{
    u8 MAXREG = MACHINE_REGISTER_COUNT - 2;
    metadata->reservedRegisters[0] = TEMP_0;
    metadata->touchedRegisters[TEMP_0] = 1;
    metadata->reservedRegisters[1] = TEMP_1;
    metadata->touchedRegisters[TEMP_1] = 1;
    metadata->reservedRegisters[2] = TEMP_2;
    metadata->touchedRegisters[TEMP_1] = 1;

    metadata->reservedRegisterCount = 3;

    metadata->registerLifetimes = LinkedList_New();

    // iterate all TAC indices, remove variables we must spill from contention for a register
    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
        struct LinkedListNode *runner = activeLifetimesThisIndex->head;
        while (runner != NULL)
        {
            struct Lifetime *examinedLifetime = runner->data;
            runner = runner->next; // drive the iterator here so our potential linkedlist_delete below doesn't invalidate/free it

            if (examinedLifetime->wbLocation != wb_unknown)
            {
                // remove (from all indices) the lifetime we will no longer consider for a register
                for (size_t removeTacIndex = 0; removeTacIndex <= metadata->largestTacIndex; removeTacIndex++)
                {
                    if (LinkedList_Find(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, examinedLifetime->name) != NULL)
                    {
                        LinkedList_Delete(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, examinedLifetime->name);
                    }
                }
            }
        }
    }

    // iterate all TAC indices
    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
        // while too many lifetimes are in contention for a register at this index
        while (activeLifetimesThisIndex->size > MAXREG)
        {
            // loop over all lifetimes, find the one with the best heuristic to spill
            struct Lifetime *bestToSpill = NULL;
            i32 bestHeuristic = INT32_MAX;
            for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
            {
                struct Lifetime *examinedLifetime = runner->data;
                i32 thisHeuristic = lifetimeHeuristic(examinedLifetime);
                if (thisHeuristic < bestHeuristic)
                {
                    bestToSpill = examinedLifetime;
                    bestHeuristic = thisHeuristic;
                }
            }

            if (bestToSpill == NULL)
            {
                ErrorAndExit(ERROR_INTERNAL, "Couldn't choose lifetime to spill!\n");
            }

            bestToSpill->wbLocation = wb_stack;
            // remove (from all indices) the lifetime we will no longer consider for a register
            for (size_t removeTacIndex = 0; removeTacIndex <= metadata->largestTacIndex; removeTacIndex++)
            {
                if (LinkedList_Find(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, bestToSpill->name) != NULL)
                {
                    LinkedList_Delete(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, bestToSpill->name);
                }
            }
        }
    }

    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
        for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
        {
            struct Lifetime *addedLifetime = runner->data;

            if (LinkedList_Find(metadata->registerLifetimes, compareLifetimes, addedLifetime->name) == NULL)
            {
                addedLifetime->wbLocation = wb_register;
                LinkedList_Append(metadata->registerLifetimes, addedLifetime);
            }
        }
    }

    if (currentVerbosity == VERBOSITY_MAX)
    {
        printf("These lifetimes get a register: ");
        for (struct LinkedListNode *runner = metadata->registerLifetimes->head; runner != NULL; runner = runner->next)
        {
            struct Lifetime *getsRegister = runner->data;
            printf("%s, ", getsRegister->name);
        }
        printf("\n");
    }
}

void assignRegisters(struct CodegenMetadata *metadata)
{
    // printf("\nassigning registers\n");
    // flag registers in use at any given TAC index so we can easily assign
    char registers[MACHINE_REGISTER_COUNT];
    struct Lifetime *occupiedBy[MACHINE_REGISTER_COUNT];

    for (u8 reg = 0; reg < MACHINE_REGISTER_COUNT; reg++)
    {
        registers[reg] = 0;
        occupiedBy[reg] = NULL;
    }

    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        u8 scannedRegister = START_ALLOCATING_FROM;

        // free any registers inhabited by expired lifetimes
        for (; scannedRegister < MACHINE_REGISTER_COUNT; scannedRegister++)
        {
            if (occupiedBy[scannedRegister] != NULL && occupiedBy[scannedRegister]->end <= tacIndex)
            {
                // printf("%s expires at %d\n", occupiedBy[j]->name, i);
                registers[scannedRegister] = 0;
                occupiedBy[scannedRegister] = NULL;
            }
        }

        // iterate all lifetimes and assign newly-live ones to a register
        for (struct LinkedListNode *ltRunner = metadata->registerLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
        {
            struct Lifetime *thisLifetime = ltRunner->data;
            if ((thisLifetime->start == tacIndex) && // if the lifetime starts at this step
                (!thisLifetime->inRegister))  // lifetime doesn't yet have a register
            {
                char registerFound = 0;
                // scan through all registers, looking for an unoccupied one
                for (u8 reg = START_ALLOCATING_FROM; reg < MACHINE_REGISTER_COUNT; reg++)
                {
                    if (registers[reg] == 0)
                    {
                        // printf("\tAssign register %d for variable %s\n", k, thisLifetime->name);
                        thisLifetime->registerLocation = reg;
                        thisLifetime->inRegister = 1;
                        thisLifetime->onStack = 0;

                        registers[reg] = 1;
                        occupiedBy[reg] = thisLifetime;
                        metadata->touchedRegisters[reg] = 1;
                        registerFound = 1;
                        break;
                    }
                }
                // no unoccupied register found (redundancy check)
                if (!registerFound)
                {
                    /*
                     * if we hit this, either:
                     * 1: something messed up in this function and we ended up with no register to assign this lifetime to
                     * 2: something messed up before we got to this function and too many concurrent lifetimes have been allowed to expect a register assignment
                     */

                    ErrorAndExit(ERROR_INTERNAL, "Unable to find register for variable %s!\n", thisLifetime->name);
                }
            }
        }
    }

    LinkedList_Free(metadata->registerLifetimes, NULL);
    metadata->registerLifetimes = NULL;
}

void assignStackSpace(struct CodegenMetadata *metadata)
{
    struct Stack *needStackSpace = Stack_New();
    for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *examined = runner->data;
        // we assign stack space after allocating registers for local variables
        // if we determine it makes sense to have the address of a local stack variable in a register, we still need stack space for it
        if (examined->wbLocation == wb_stack)
        {
            Stack_Push(needStackSpace, examined);
        }
    }

    if (currentVerbosity > VERBOSITY_SILENT)
    {
        printf("%d variables need stack space\n", needStackSpace->size);
    }

    // simple bubble sort the things that need stack space by their size
    for (size_t i = 0; i < needStackSpace->size; i++)
    {
        for (size_t j = 0; j < needStackSpace->size - i - 1; j++)
        {
            struct Lifetime *thisLifetime = needStackSpace->data[j];

            size_t thisSize = Scope_getSizeOfType(metadata->function->mainScope, &thisLifetime->type);
            size_t compSize = Scope_getSizeOfType(metadata->function->mainScope, &(((struct Lifetime *)needStackSpace->data[j + 1])->type));

            if (thisSize < compSize)
            {
                struct Lifetime *swap = needStackSpace->data[j];
                needStackSpace->data[j] = needStackSpace->data[j + 1];
                needStackSpace->data[j + 1] = swap;
            }
        }
    }

    for (size_t lifetimeIndex = 0; lifetimeIndex < needStackSpace->size; lifetimeIndex++)
    {
        struct Lifetime *thisLifetime = needStackSpace->data[lifetimeIndex];
        if (thisLifetime->isArgument)
        {
            struct VariableEntry *argumentEntry = Scope_lookupVarByString(metadata->function->mainScope, thisLifetime->name);
            thisLifetime->stackLocation = argumentEntry->stackOffset;
        }
        else
        {
            metadata->localStackSize += Scope_ComputePaddingForAlignment(metadata->function->mainScope, &thisLifetime->type, metadata->localStackSize);
            metadata->localStackSize += Scope_getSizeOfType(metadata->function->mainScope, &thisLifetime->type);
            thisLifetime->stackLocation = -1 * metadata->localStackSize;
        }
    }

    while (metadata->localStackSize % MACHINE_REGISTER_SIZE_BYTES)
    {
        metadata->localStackSize++;
    }

    if (currentVerbosity > VERBOSITY_SILENT)
    {
        printf("Stack slots assigned for spilled/stack variables\n");
    }

    Stack_Free(needStackSpace);
}

void allocateRegisters(struct CodegenMetadata *metadata)
{
    metadata->allLifetimes = findLifetimes(metadata->function->mainScope, metadata->function->BasicBlockList);

    // find all overlapping lifetimes, to figure out which variables can live in registers vs being spilled
    metadata->largestTacIndex = 0;
    for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *thisLifetime = runner->data;
        if (thisLifetime->end > metadata->largestTacIndex)
        {
            metadata->largestTacIndex = thisLifetime->end;
        }
    }

    // any local objects must live throughout the entire function since we currently can't know their true lifetime
    // if we have uint32 array[123]; uint32 *pointer = array; then pointer extends the lifetime of array and we don't track it currently
    for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *examined = runner->data;
        if (examined->wbLocation == wb_stack)
        {
            examined->end = metadata->largestTacIndex;
        }
    }

    // generate an array of lists corresponding to which lifetimes are active at a given TAC step by index in the array
    metadata->lifetimeOverlaps = malloc((metadata->largestTacIndex + 1) * sizeof(struct LinkedList *));
    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        metadata->lifetimeOverlaps[tacIndex] = LinkedList_New();
    }

    size_t mostConcurrentLifetimes = generateLifetimeOverlaps(metadata);

    // printf("at most %d concurrent lifetimes\n", mostConcurrentLifetimes);

    selectRegisterVariables(metadata, mostConcurrentLifetimes);

    // printf("selected which variables get registers\n");
    assignRegisters(metadata);

    if (currentVerbosity > VERBOSITY_SILENT)
    {
        printf("assigned registers\n");
    }

    if (currentVerbosity == VERBOSITY_MAX)
    {
        printf("\nLifetimes for %s\n", metadata->function->name);
        for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
        {
            struct Lifetime *examinedLifetime = runner->data;
            char wbLocName = '?';
            switch (examinedLifetime->wbLocation)
            {
            case wb_register:
                wbLocName = 'R';
                break;

            case wb_stack:
                wbLocName = 'S';
                break;

            case wb_global:
                wbLocName = 'G';
                break;

            case wb_unknown:
                wbLocName = '?';
                break;
            }
            char *typeName = Type_GetName(&examinedLifetime->type);
            printf("%40s (%10s)(wb:%c)(%2zu-%2zu): ", examinedLifetime->name, typeName, wbLocName,
                   examinedLifetime->start, examinedLifetime->end);
            free(typeName);
            for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
            {
                if (tacIndex >= examinedLifetime->start && tacIndex <= examinedLifetime->end)
                {
                    printf("*");
                }
                else
                {
                    printf(" ");
                }
            }
            printf("\n");
        }
    }

    assignStackSpace(metadata);

    if (currentVerbosity == VERBOSITY_MAX)
    {
        // print the function's stack footprint
        {
            struct Stack *stackLayout = Stack_New();
            for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
            {
                struct Lifetime *examined = runner->data;
                if (examined->onStack)
                {
                    Stack_Push(stackLayout, examined);
                }
            }

            // simple bubble sort the things that are on the stack by their address
            for (size_t i = 0; i < stackLayout->size; i++)
            {
                for (size_t j = 0; j < stackLayout->size - i - 1; j++)
                {
                    struct Lifetime *thisLifetime = stackLayout->data[j];

                    ssize_t thisAddr = thisLifetime->stackLocation;
                    ssize_t compAddr = ((struct Lifetime *)stackLayout->data[j + 1])->stackLocation;

                    if (thisAddr < compAddr)
                    {
                        struct Lifetime *swap = stackLayout->data[j];
                        stackLayout->data[j] = stackLayout->data[j + 1];
                        stackLayout->data[j + 1] = swap;
                    }
                }
            }

            char crossedZero = 0;
            for (size_t lifetimeIndex = 0; lifetimeIndex < stackLayout->size; lifetimeIndex++)
            {
                struct Lifetime *thisLifetime = stackLayout->data[lifetimeIndex];
                if ((!crossedZero) && (thisLifetime->stackLocation < 0))
                {
                    printf("SAVED BP\nSAVED BP\nSAVED BP\nSAVED BP\n");
                    printf("RETURN ADDRESSS\nRETURN ADDRESSS\nRETURN ADDRESSS\nRETURN ADDRESSS\n");
                    printf("---------BASE POINTER POINTS HERE--------\n");
                    crossedZero = 1;
                }
                size_t size = Scope_getSizeOfType(metadata->function->mainScope, &thisLifetime->type);
                if (thisLifetime->type.arraySize > 0)
                {
                    size_t elementSize = size / thisLifetime->type.arraySize;
                    for (size_t j = 0; j < size; j++)
                    {
                        printf("%s[%lu]\n", thisLifetime->name, j / elementSize);
                    }
                }
                else
                {
                    for (size_t j = 0; j < size; j++)
                    {
                        printf("%s\n", thisLifetime->name);
                    }
                }
            }

            Stack_Free(stackLayout);
        }
    }

    if (currentVerbosity > VERBOSITY_SILENT)
    {
        printf("Final roundup of variables and where they live:\n");
        printf("Local stack footprint: %zu bytes\n", metadata->localStackSize);
        for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
        {
            struct Lifetime *examined = runner->data;
            if (examined->inRegister)
            {
                if (examined->onStack)
                {
                    printf("&%-19s: %%r%d\n", examined->name, examined->registerLocation);
                }
                else
                {
                    printf("%-20s: %%r%d\n", examined->name, examined->registerLocation);
                }
            }
            if (examined->onStack)
            {
                if (examined->stackLocation > 0)
                {
                    printf("%-20s: %%bp+%2zd - %%bp+%2zd\n", examined->name, examined->stackLocation, examined->stackLocation + Scope_getSizeOfType(metadata->function->mainScope, &examined->type));
                }
                else
                {
                    printf("%-20s: %%bp%2zd - %%bp%2zd\n", examined->name, examined->stackLocation, examined->stackLocation + Scope_getSizeOfType(metadata->function->mainScope, &examined->type));
                }
            }
        }
    }


    for (u8 reg = START_ALLOCATING_FROM; reg < MACHINE_REGISTER_COUNT; reg++)
    {
        if (metadata->touchedRegisters[reg])
        {
            metadata->calleeSaveStackSize += MACHINE_REGISTER_SIZE_BYTES;
            metadata->nRegistersCalleeSaved++;
        }
    }

    // make room to save frame pointer
    metadata->calleeSaveStackSize += MACHINE_REGISTER_SIZE_BYTES;
    metadata->nRegistersCalleeSaved++;

    // if this function calls another function or is an asm function, make space to store the frame pointer and return address
    if (metadata->function->callsOtherFunction || metadata->function->isAsmFun)
    {
        metadata->calleeSaveStackSize += MACHINE_REGISTER_SIZE_BYTES;
        metadata->nRegistersCalleeSaved++;
    }

    metadata->totalStackSize = metadata->localStackSize + metadata->calleeSaveStackSize;
    while (metadata->totalStackSize % STACK_ALIGN_BYTES)
    {
        metadata->totalStackSize++;
    }
}
