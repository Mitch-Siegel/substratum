#include "regalloc.h"

#include <string.h>

#include "codegen_generic.h"
#include "log.h"
#include "regalloc_generic.h"
#include "symtab.h"
#include "util.h"

// return the heuristic for how good a given lifetime is to spill - lower is better
size_t lifetimeHeuristic(struct Lifetime *lifetime)
{
    const size_t argumentSpillHeuristicMultiplier = 10;
    // base heuristic is lifetime length
    size_t heuristic = lifetime->end - lifetime->start;
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

void printRegisterLifetimes(struct CodegenMetadata *metadata)
{
    Log(LOG_DEBUG, "These lifetimes get a register: ");
    for (struct LinkedListNode *runner = metadata->registerLifetimes->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *getsRegister = runner->data;
        Log(LOG_DEBUG, "\t%s", getsRegister->name);
    }
}

void removeNonContendingLifetimes(struct CodegenMetadata *metadata)
{
    // iterate all TAC indices, remove variables not in contention for a register
    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
        struct LinkedListNode *runner = activeLifetimesThisIndex->head;
        while (runner != NULL)
        {
            struct Lifetime *examinedLifetime = runner->data;
            runner = runner->next; // drive the iterator here so our potential linkedlist_delete below doesn't invalidate/free it

            // if we already know where the lifetime will end up, it is trivially not in contention for a register
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
}

// spill the lifetime with the most optimal heuristic from activeLifetimesThisIndex
void spillAtIndex(struct CodegenMetadata *metadata, struct LinkedList *activeLifetimesThisIndex)
{
    // loop over all lifetimes, find the one with the best heuristic to spill
    struct Lifetime *bestToSpill = NULL;
    size_t bestHeuristic = SIZE_MAX;
    for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *examinedLifetime = runner->data;
        size_t thisHeuristic = lifetimeHeuristic(examinedLifetime);
        if (thisHeuristic < bestHeuristic)
        {
            bestToSpill = examinedLifetime;
            bestHeuristic = thisHeuristic;
        }
    }

    if (bestToSpill == NULL)
    {
        InternalError("Couldn't choose lifetime to spill!");
    }

    // now that it's not in contention for a register, we know it will go on the stack
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

    // remove any lifetimes which we already know the location of
    removeNonContendingLifetimes(metadata);

    // iterate all TAC indices, selecting which lifetimes will be spilled if too many are in contention for registers
    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
        // while too many lifetimes are in contention for a register at this index
        while (activeLifetimesThisIndex->size > MAXREG)
        {
            spillAtIndex(metadata, activeLifetimesThisIndex);
        }
    }

    // at this point, we know that we have at least enough registers to allocate one for each lifetime remaining in metadata->lifetimeOverlaps
    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
        for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
        {
            struct Lifetime *addedLifetime = runner->data;
            // if we find a a lifetime which has not been added to the registerLifetimes list, put it there
            if (LinkedList_Find(metadata->registerLifetimes, compareLifetimes, addedLifetime->name) == NULL)
            {
                addedLifetime->wbLocation = wb_register;
                LinkedList_Append(metadata->registerLifetimes, addedLifetime);
            }
        }
    }

    printRegisterLifetimes(metadata);
}

void freeExpiringRegisters(u8 registers[MACHINE_REGISTER_COUNT], struct Lifetime *occupiedBy[MACHINE_REGISTER_COUNT], size_t index)
{
    u8 scannedRegister = START_ALLOCATING_FROM;

    // free any registers inhabited by expired lifetimes
    for (; scannedRegister < MACHINE_REGISTER_COUNT; scannedRegister++)
    {
        if (occupiedBy[scannedRegister] != NULL && occupiedBy[scannedRegister]->end <= index)
        {
            Log(LOG_DEBUG, "%s expires at %d", occupiedBy[scannedRegister]->name, scannedRegister);
            registers[scannedRegister] = 0;
            occupiedBy[scannedRegister] = NULL;
        }
    }
}

void assignRegisters(struct CodegenMetadata *metadata)
{
    Log(LOG_INFO, "Assign registers for function %s", metadata->function->name);
    // flag registers in use at any given TAC index so we can easily assign
    u8 registers[MACHINE_REGISTER_COUNT];
    struct Lifetime *occupiedBy[MACHINE_REGISTER_COUNT];

    for (u8 reg = 0; reg < MACHINE_REGISTER_COUNT; reg++)
    {
        registers[reg] = 0;
        occupiedBy[reg] = NULL;
    }

    for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        // free up registers which contain lifetimes expiring at this index
        freeExpiringRegisters(registers, occupiedBy, tacIndex);

        // iterate all lifetimes and assign newly-live ones to a register
        for (struct LinkedListNode *ltRunner = metadata->registerLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
        {
            struct Lifetime *thisLifetime = ltRunner->data;
            if ((thisLifetime->start == tacIndex) && // if the lifetime starts at this step
                (!thisLifetime->inRegister))         // lifetime doesn't yet have a register
            {
                char registerFound = 0;
                // scan through all registers, looking for an unoccupied one
                for (u8 reg = START_ALLOCATING_FROM; reg < MACHINE_REGISTER_COUNT; reg++)
                {
                    if (registers[reg] == 0)
                    {
                        Log(LOG_DEBUG, "Assign register %s for variable %s", registerNames[reg], thisLifetime->name);
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

                    InternalError("Unable to find register for variable %s!", thisLifetime->name);
                }
            }
        }
    }

    LinkedList_Free(metadata->registerLifetimes, NULL);
    metadata->registerLifetimes = NULL;
}

void assignStackSpace(struct CodegenMetadata *metadata)
{
    Log(LOG_INFO, "Assign stack space for function %s", metadata->function->name);
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

    Log(LOG_DEBUG, "%zu variables need stack space", needStackSpace->size);

    // simple bubble sort the things that need stack space by their size
    for (size_t i = 0; i < needStackSpace->size; i++)
    {
        for (size_t j = 0; j < needStackSpace->size - i - 1; j++)
        {
            struct Lifetime *thisLifetime = needStackSpace->data[j];

            size_t thisSize = Type_GetSize(&thisLifetime->type, metadata->function->mainScope);
            size_t compSize = Type_GetSize(&(((struct Lifetime *)needStackSpace->data[j + 1])->type), metadata->function->mainScope);

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
            struct VariableEntry *argumentEntry = lookupVarByString(metadata->function->mainScope, thisLifetime->name);
            thisLifetime->stackLocation = argumentEntry->stackOffset;
        }
        else
        {
            metadata->localStackSize += Scope_ComputePaddingForAlignment(metadata->function->mainScope, &thisLifetime->type, metadata->localStackSize);
            metadata->localStackSize += Type_GetSize(&thisLifetime->type, metadata->function->mainScope);
            if (metadata->localStackSize > I64_MAX)
            {
                // TODO: implementation dependent size of size_t
                InternalError("Function %s has arg stack size too large (%zd bytes)!", metadata->function->name, metadata->localStackSize);
            }
            thisLifetime->stackLocation = -1 * (ssize_t)metadata->localStackSize;
        }
    }

    while (metadata->localStackSize % MACHINE_REGISTER_SIZE_BYTES)
    {
        metadata->localStackSize++;
    }

    Log(LOG_DEBUG, "Stack slots assigned for spilled/stack variables");

    Stack_Free(needStackSpace);
}

void printLifetimes(struct CodegenMetadata *metadata)
{
    Log(LOG_DEBUG, "Lifetimes for %s", metadata->function->name);
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

        size_t lineLen = 40 + strlen(typeName) + 30 + metadata->largestTacIndex;
        char *lifetimeLine = malloc(lineLen + 1);

        u32 startIndex = snprintf(lifetimeLine, lineLen, "%40s (%10s)(wb:%c)(%3zu-%3zu)", examinedLifetime->name, typeName, wbLocName, examinedLifetime->stackLocation, examinedLifetime->end);
        free(typeName);
        for (size_t tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
        {
            if (tacIndex >= examinedLifetime->start && tacIndex <= examinedLifetime->end)
            {
                lineLen += sprintf(lifetimeLine + startIndex, "*");
            }
            else
            {
                lineLen += sprintf(lifetimeLine + startIndex, " ");
            }
        }

        Log(LOG_DEBUG, lifetimeLine);
        free(lifetimeLine);
    }
}

void printStackFootprint(struct CodegenMetadata *metadata)
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
                for (size_t byteIndex = 0; byteIndex < sizeof(size_t); byteIndex++)
                {
                    Log(LOG_DEBUG, "SAVED BP");
                }
                for (size_t byteIndex = 0; byteIndex < sizeof(size_t); byteIndex++)
                {
                    Log(LOG_DEBUG, "RETURN ADDRESS");
                }
                Log(LOG_DEBUG, "---------BASE POINTER POINTS HERE--------");
                crossedZero = 1;
            }
            size_t size = Type_GetSize(&thisLifetime->type, metadata->function->mainScope);
            char *typeName = Type_GetName(&thisLifetime->type);
            for (size_t lineIndex = 0; lineIndex < size; lineIndex++)
            {
                Log(LOG_DEBUG, "%s", typeName);
            }
            free(typeName);
        }

        Stack_Free(stackLayout);
    }
}

void printVariableLocations(struct CodegenMetadata *metadata)
{
    Log(LOG_DEBUG, "Final roundup of variables and where they live:");
    Log(LOG_DEBUG, "Local stack footprint: %zu bytes", metadata->localStackSize);
    for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *examined = runner->data;
        if (examined->inRegister)
        {
            if (examined->onStack)
            {
                Log(LOG_DEBUG, "&%-19s: %%r%d", examined->name, examined->registerLocation);
            }
            else
            {
                Log(LOG_DEBUG, "%-20s: %%r%d", examined->name, examined->registerLocation);
            }
        }
        if (examined->onStack)
        {
            if (examined->stackLocation > 0)
            {
                Log(LOG_DEBUG, "%-20s: %%bp+%2zd - %%bp+%2zd", examined->name, examined->stackLocation, examined->stackLocation + Type_GetSize(&examined->type, metadata->function->mainScope));
            }
            else
            {
                Log(LOG_DEBUG, "%-20s: %%bp%2zd - %%bp%2zd", examined->name, examined->stackLocation, examined->stackLocation + Type_GetSize(&examined->type, metadata->function->mainScope));
            }
        }
    }
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

    Log(LOG_DEBUG, "at most %d concurrent lifetimes", mostConcurrentLifetimes);

    selectRegisterVariables(metadata, mostConcurrentLifetimes);

    assignRegisters(metadata);

    Log(LOG_DEBUG, "assigned registers");

    printLifetimes(metadata);

    assignStackSpace(metadata);

    printStackFootprint(metadata);
    printVariableLocations(metadata);

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
