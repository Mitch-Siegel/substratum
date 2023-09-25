#include "regalloc_opt0.h"

// return the heuristic for how good a given lifetime is to spill - lower is better
int lifetimeHeuristic_0(struct Lifetime *lt)
{
    // base heuristic is lifetime length
    int h = lt->end - lt->start;
    // add the number of reads for this variable since they have some cost
    h += lt->nreads;
    // multiply by number of writes for this variable since that is a high-cost operation
    h *= lt->nwrites;

    // inflate heuristics for cases which have no actual stack space cost to spill:
    // super-prefer to "spill" arguments as they already have a stack address
    if (!lt->isArgument)
    {
        h *= 10;
    }

    return h;
}

void selectRegisterVariables(struct CodegenMetadata *metadata, int mostConcurrentLifetimes)
{
    int MAXREG = REGISTERS_TO_ALLOCATE - 2;
    metadata->reservedRegisters[0] = SCRATCH_REGISTER;
    metadata->touchedRegisters[SCRATCH_REGISTER] = 1;
    metadata->reservedRegisters[1] = RETURN_REGISTER;
    metadata->touchedRegisters[SECOND_SCRATCH_REGISTER] = 1;
    metadata->reservedRegisters[2] = SECOND_SCRATCH_REGISTER;

    metadata->reservedRegisterCount = 3;

    metadata->registerLifetimes = LinkedList_New();

    // iterate all TAC indices, remove variables we must spill from contention for a register
    for (int tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
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
                for (int removeTacIndex = 0; removeTacIndex <= metadata->largestTacIndex; removeTacIndex++)
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
    for (int tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
    {
        struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
        // while too many lifetimes are in contention for a register at this index
        while (activeLifetimesThisIndex->size > MAXREG)
        {
            // loop over all lifetimes, find the one with the best heuristic to spill
            struct Lifetime *bestToSpill = NULL;
            int bestHeuristic = __INT_MAX__;
            for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
            {
                struct Lifetime *examinedLifetime = runner->data;
                int thisHeuristic = lifetimeHeuristic_0(examinedLifetime);
                if (thisHeuristic < bestHeuristic)
                {
                    bestToSpill = examinedLifetime;
                    bestHeuristic = thisHeuristic;
                }
            }

            bestToSpill->wbLocation = wb_stack;
            // remove (from all indices) the lifetime we will no longer consider for a register
            for (int removeTacIndex = 0; removeTacIndex <= metadata->largestTacIndex; removeTacIndex++)
            {
                if (LinkedList_Find(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, bestToSpill->name) != NULL)
                {
                    LinkedList_Delete(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, bestToSpill->name);
                }
            }
        }
    }

    for (int tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
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
    char registers[REGISTERS_TO_ALLOCATE];
    struct Lifetime *occupiedBy[REGISTERS_TO_ALLOCATE];

    for (int i = 0; i < REGISTERS_TO_ALLOCATE; i++)
    {
        registers[i] = 0;
        occupiedBy[i] = NULL;
        metadata->touchedRegisters[i] = 0;
    }

    for (int i = 0; i <= metadata->largestTacIndex; i++)
    {
        int j = metadata->reservedRegisterCount - 1;
        if (metadata->reservedRegisterCount == 0)
        {
            j = 0;
        }

        // free any registers inhabited by expired lifetimes
        for (; j < REGISTERS_TO_ALLOCATE; j++)
        {
            if (occupiedBy[j] != NULL && occupiedBy[j]->end <= i)
            {
                // printf("%s expires at %d\n", occupiedBy[j]->name, i);
                registers[j] = 0;
                occupiedBy[j] = NULL;
            }
        }

        // iterate all lifetimes and assign newly-live ones to a register
        for (struct LinkedListNode *ltRunner = metadata->registerLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
        {
            struct Lifetime *thisLifetime = ltRunner->data;
            if ((thisLifetime->start == i) && // if the lifetime starts at this step
                (!thisLifetime->inRegister))  // lifetime doesn't yet have a register
            {
                char registerFound = 0;
                // scan through all registers, looking for an unoccupied one
                for (int j = metadata->reservedRegisterCount - 1; j < REGISTERS_TO_ALLOCATE; j++)
                {
                    if (registers[j] == 0)
                    {
                        // printf("\tAssign register %d for variable %s\n", j, thisLifetime->name);
                        thisLifetime->registerLocation = j;
                        thisLifetime->inRegister = 1;
                        thisLifetime->onStack = 0;

                        registers[j] = 1;
                        occupiedBy[j] = thisLifetime;
                        metadata->touchedRegisters[j] = 1;
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

int assignStackSpace(struct CodegenMetadata *m)
{
    int localStackFootprint = 0;

    struct Stack *needStackSpace = Stack_New();
    for (struct LinkedListNode *runner = m->allLifetimes->head; runner != NULL; runner = runner->next)
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
    for (int i = 0; i < needStackSpace->size; i++)
    {
        for (int j = 0; j < needStackSpace->size - i - 1; j++)
        {
            struct Lifetime *thisLifetime = needStackSpace->data[j];

            int thisSize = Scope_getSizeOfType(m->function->mainScope, &thisLifetime->type);
            int compSize = Scope_getSizeOfType(m->function->mainScope, &(((struct Lifetime *)needStackSpace->data[j + 1])->type));

            if (thisSize < compSize)
            {
                struct Lifetime *swap = needStackSpace->data[j];
                needStackSpace->data[j] = needStackSpace->data[j + 1];
                needStackSpace->data[j + 1] = swap;
            }
        }
    }


    for (int i = 0; i < needStackSpace->size; i++)
    {
        struct Lifetime *thisLifetime = needStackSpace->data[i];
        if (thisLifetime->isArgument)
        {
            struct VariableEntry *argumentEntry = Scope_lookupVarByString(m->function->mainScope, thisLifetime->name);
            thisLifetime->stackLocation = argumentEntry->stackOffset;
        }
        else
        {
            localStackFootprint -= Scope_getSizeOfType(m->function->mainScope, &thisLifetime->type);
            thisLifetime->stackLocation = localStackFootprint;
        }
    }

    if(currentVerbosity > VERBOSITY_SILENT)
    {
        printf("Stack slots assigned for spilled/stack variables\n");
    }

    Stack_Free(needStackSpace);
    return localStackFootprint;
}

int allocateRegisters_0(struct CodegenMetadata *metadata)
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
    for (int i = 0; i <= metadata->largestTacIndex; i++)
    {
        metadata->lifetimeOverlaps[i] = LinkedList_New();
    }

    int mostConcurrentLifetimes = generateLifetimeOverlaps(metadata);

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
            printf("%40s (%10s)(wb:%c)(%2d-%2d): ", examinedLifetime->name, typeName, wbLocName,
                   examinedLifetime->start, examinedLifetime->end);
            free(typeName);
            for (int i = 0; i <= metadata->largestTacIndex; i++)
            {
                if (i >= examinedLifetime->start && i <= examinedLifetime->end)
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

    int localStackFootprint = -1 * assignStackSpace(metadata);

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
            for (int i = 0; i < stackLayout->size; i++)
            {
                for (int j = 0; j < stackLayout->size - i - 1; j++)
                {
                    struct Lifetime *thisLifetime = stackLayout->data[j];

                    int thisAddr = thisLifetime->stackLocation;
                    int compAddr = ((struct Lifetime *)stackLayout->data[j + 1])->stackLocation;

                    if (thisAddr < compAddr)
                    {
                        struct Lifetime *swap = stackLayout->data[j];
                        stackLayout->data[j] = stackLayout->data[j + 1];
                        stackLayout->data[j + 1] = swap;
                    }
                }
            }

            char crossedZero = 0;
            for (int i = 0; i < stackLayout->size; i++)
            {
                struct Lifetime *thisLifetime = stackLayout->data[i];
                if ((!crossedZero) && (thisLifetime->stackLocation < 0))
                {
                    printf("SAVED BP\nSAVED BP\nSAVED BP\nSAVED BP\n");
                    printf("RETURN ADDRESSS\nRETURN ADDRESSS\nRETURN ADDRESSS\nRETURN ADDRESSS\n");
                    printf("---------BASE POINTER POINTS HERE--------\n");
                    crossedZero = 1;
                }
                int size = Scope_getSizeOfType(metadata->function->mainScope, &thisLifetime->type);
                if (thisLifetime->type.arraySize > 0)
                {
                    int elementSize = size / thisLifetime->type.arraySize;
                    for (int j = 0; j < size; j++)
                    {
                        printf("%s[%d]\n", thisLifetime->name, j / elementSize);
                    }
                }
                else
                {
                    for (int j = 0; j < size; j++)
                    {
                        printf("%s\n", thisLifetime->name);
                    }
                }
            }
        }
    }

    if (currentVerbosity > VERBOSITY_SILENT)
    {
        printf("Final roundup of variables and where they live:\n");
        printf("Local stack footprint: %d bytes\n", localStackFootprint);
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
                    printf("%-20s: %%bp+%2d - %%bp+%2d\n", examined->name, examined->stackLocation, examined->stackLocation + Scope_getSizeOfType(metadata->function->mainScope, &examined->type));
                }
                else
                {
                    printf("%-20s: %%bp%2d - %%bp%2d\n", examined->name, examined->stackLocation, examined->stackLocation + Scope_getSizeOfType(metadata->function->mainScope, &examined->type));
                }
            }
        }
    }

    return localStackFootprint;
}
