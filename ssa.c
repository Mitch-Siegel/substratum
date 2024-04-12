#include "ssa.h"
#include "symtab.h"

#include "livevars.h"

int compareBlockNumbers(void *numberA, void *numberB)
{
    return (ssize_t)numberA != (ssize_t)numberB;
}

size_t hashTacOperand(void *operand)
{
    size_t hash = 0;
    for (size_t byteIndex = 0; byteIndex < sizeof(struct TACOperand); byteIndex++)
    {
        hash += ((u8 *)operand)[byteIndex];
        hash <<= 1;
    }
    return hash;
}

void printControlFlowsAsDot(struct Idfa *idfa, char *functionName)
{
    printf("digraph %s{\nedge[dir=forward]\nnode[shape=plaintext,style=filled]\n", functionName);
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        for (struct LinkedListNode *flowRunner = idfa->context->successors[blockIndex]->elements->head; flowRunner != NULL; flowRunner = flowRunner->next)
        {
            struct BasicBlock *destinationBlock = flowRunner->data;
            printf("%s_%zu:s->%s_%zu:n\n", functionName, blockIndex, functionName, destinationBlock->labelNum);
        }

        struct BasicBlock *thisBlock = idfa->context->blocks[blockIndex];
        printf("%s_%zu[label=<%s_%zu<BR />\n", functionName, thisBlock->labelNum, functionName, thisBlock->labelNum);

        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            char *tacString = sPrintTACLine(tacRunner->data);
            printf("%s<BR />\n", tacString);
            free(tacString);
        }
        printf(">]\n");
    }

    printf("}\n\n\n");
}

void insertPhiFunctions(struct Idfa *liveVars)
{
    struct AST fakePhiTree;
    memset(&fakePhiTree, 0, sizeof(struct AST));

    // iterate all blocks
    for (size_t blockIndex = 0; blockIndex < liveVars->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *phiBlock = liveVars->context->blocks[blockIndex];
        size_t blockEntryTacIndex = 0;
        if (phiBlock->TACList->size > 0)
        {
            blockEntryTacIndex = ((struct TACLine *)phiBlock->TACList->head->data)->index;
        }
        // hash table to map from TAC operand -> count of number of predecessor blocks the variable is live out from
        struct HashTable *phiVars = HashTable_New(1, hashTacOperand, compareTacOperand, NULL, free);
        // iterate all predecessor blocks
        for (struct LinkedListNode *predecessorRunner = liveVars->context->predecessors[blockIndex]->elements->head; predecessorRunner != NULL; predecessorRunner = predecessorRunner->next)
        {
            struct BasicBlock *predecessorBlock = predecessorRunner->data;
            // iterate all live vars out facts from the predecessor
            for (struct LinkedListNode *liveVarRunner = liveVars->facts.out[predecessorBlock->labelNum]->elements->head; liveVarRunner != NULL; liveVarRunner = liveVarRunner->next)
            {
                struct TACOperand *liveOut = liveVarRunner->data;

                // temporary variables generated during linearization are inherently single-assignment by their very nature
                if(liveOut->permutation == vp_temp)
                {
                    continue;
                }

                size_t *liveOutCount = HashTable_Lookup(phiVars, liveOut);
                if (liveOutCount == NULL)
                {
                    // if it's the first time we've seen this operand as live out, allocate a new count for it
                    liveOutCount = malloc(sizeof(size_t));
                    memset(liveOutCount, 0, sizeof(size_t));
                    HashTable_Insert(phiVars, liveOut, liveOutCount);
                }
                // increment the count of number of times we've seen this variable live out of a predecessor
                (*liveOutCount)++;
            }
        }

        // iterate all entries in the hash table we created in the loop above
        for (struct LinkedListNode *phiVarRunner = phiVars->buckets[0]->elements->head; phiVarRunner != NULL; phiVarRunner = phiVarRunner->next)
        {
            struct HashTableEntry *phiVarEntry = phiVarRunner->data;
            size_t *nBlocksLiveInFrom = phiVarEntry->value;
            // for any variables which are live out of more than one predecessor block
            if (*nBlocksLiveInFrom > 1)
            {
                // predecrement so we can insert exactly the right number of phis
                (*nBlocksLiveInFrom)--;
                struct TACOperand *phiVar = phiVarEntry->key;

                // insert a phi for each occurrence (- 1 because the first phi can take 2 unique operands instead of one new operand and the output of a previous phi)
                while (*nBlocksLiveInFrom > 0)
                {
                    struct TACLine *newPhi = newTACLine(blockEntryTacIndex, tt_phi, &fakePhiTree);
                    newPhi->operands[0] = *phiVar;
                    newPhi->operands[1] = *phiVar;
                    newPhi->operands[2] = *phiVar;
                    BasicBlock_prepend(phiBlock, newPhi);

                    (*nBlocksLiveInFrom)--;
                }
            }
        }

        HashTable_Free(phiVars);
    }
}

struct TACOperand *ssaOperandLookupOrInsert(struct Set *ssaOperands, struct TACOperand *originalOperand)
{
    struct TACOperand *foundOperand = Set_Find(ssaOperands, originalOperand);
    if (foundOperand == NULL)
    {
        struct TACOperand *newOperand = malloc(sizeof(struct TACOperand));
        memcpy(newOperand, originalOperand, sizeof(struct TACOperand));
        newOperand->ssaNumber = 0;
        Set_Insert(ssaOperands, newOperand);
        return newOperand;
    }
    return foundOperand;
}

// go over all TAC operands which are assigned to and give them unique SSA numbers
void renameWrittenTACOperands(struct IdfaContext *context)
{
    struct Set *ssaOperandNumbers = Set_New(compareTacOperandIgnoreSsaNumber, free);
    // make a list of blocks to traverse, use it as a queue
    struct LinkedList *blocksToTraverse = LinkedList_New();
    LinkedList_Append(blocksToTraverse, context->blocks[0]); // block 0 is always the entry of the function
    struct Set *seenBlocks = Set_New(compareBlockNumbers, NULL);
    while (blocksToTraverse->size > 0)
    {
        // grab the block at the front of the queue
        struct BasicBlock *thisBlock = LinkedList_PopFront(blocksToTraverse);

        // iterate all successors of this block
        for (struct LinkedListNode *successorRunner = context->successors[thisBlock->labelNum]->elements->head; successorRunner != NULL; successorRunner = successorRunner->next)
        {
            struct BasicBlock *successorBlock = successorRunner->data;
            // if we have not added them to the back of the queue yet, do so and mark that we have seen them
            if (Set_Find(seenBlocks, successorBlock) == NULL)
            {
                LinkedList_Append(blocksToTraverse, successorBlock);
                Set_Insert(seenBlocks, successorBlock);
            }
        }

        // iterate all TAC in this block
        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *thisTac = tacRunner->data;

            // iterate all operands in this TAC, setting the ssa number and incrementing our count so the next assignment will have a different number
            for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
            {
                if (getUseOfOperand(thisTac, operandIndex) == u_write)
                {
                    struct TACOperand *originalOperand = &thisTac->operands[operandIndex];
                    struct TACOperand *ssaOperand = ssaOperandLookupOrInsert(ssaOperandNumbers, originalOperand);
                    originalOperand->ssaNumber = ssaOperand->ssaNumber++;
                }
            }
        }
    }

    Set_Free(ssaOperandNumbers);
    LinkedList_Free(blocksToTraverse, NULL);
    Set_Free(seenBlocks);
}

struct Set *findHighestSsaLiveIns(struct Idfa *liveVars, size_t blockIndex)
{
    struct Set *highestSsaLiveIns = Set_New(compareTacOperandIgnoreSsaNumber, NULL);

    for (struct LinkedListNode *liveInRunner = liveVars->facts.in[blockIndex]->elements->head; liveInRunner != NULL; liveInRunner = liveInRunner->next)
    {
        struct TACOperand *thisLiveIn = liveInRunner->data;
        struct TACOperand *highestLiveIn = Set_Find(highestSsaLiveIns, thisLiveIn);
        if (highestLiveIn == NULL)
        {
            Set_Insert(highestSsaLiveIns, thisLiveIn);
        }
        else if (highestLiveIn->ssaNumber < thisLiveIn->ssaNumber)
        {
            Set_Delete(highestSsaLiveIns, highestLiveIn);
            Set_Insert(highestSsaLiveIns, thisLiveIn);
        }
    }

    return highestSsaLiveIns;
}

// find all SSA variables live in to block blockIndex based on tacOperand
struct Set *findAllSsaLiveInsForVariable(struct Idfa *liveVars, size_t blockIndex, struct TACOperand *operand)
{
    struct Set *matchingOperands = Set_New(compareTacOperand, NULL);
    for (struct LinkedListNode *liveInRunner = liveVars->facts.in[blockIndex]->elements->head; liveInRunner != NULL; liveInRunner = liveInRunner->next)
    {
        struct TACOperand *thisLiveIn = liveInRunner->data;
        if (compareTacOperandIgnoreSsaNumber(operand, thisLiveIn) == 0)
        {
            Set_Insert(matchingOperands, thisLiveIn);
        }
    }
    return matchingOperands;
}

struct TACOperand *lookupMostRecentSsaAssignment(struct Set *highestSsaLiveIns, struct Set *assignmentsThisBlock, struct TACOperand *originalOperand)
{
    struct TACOperand *mostRecentAssignment = NULL;
    // first, attempt to find an assignment from within the current block
    struct TACOperand *assignmentInBlock = Set_Find(assignmentsThisBlock, originalOperand);
    if (assignmentInBlock == NULL)
    {
        // no assignment within the block, so search from the set of live SSA's coming in to the block
        mostRecentAssignment = Set_Find(highestSsaLiveIns, originalOperand);
    }
    else
    {
        mostRecentAssignment = assignmentInBlock;
    }
    return mostRecentAssignment;
}

void renameReadTACOperands(struct Idfa *liveVars)
{
    // iterate all blocks
    for (size_t blockIndex = 0; blockIndex < liveVars->context->nBlocks; blockIndex++)
    {
        // the highest SSA numbers that live in to this basic block
        struct Set *highestSsaLiveIns = findHighestSsaLiveIns(liveVars, blockIndex);
        // any SSA operands which are assigned to within the block
        struct Set *ssaLivesFromThisBlock = Set_New(compareTacOperandIgnoreSsaNumber, NULL);

        // iterate all TAC in the block
        struct BasicBlock *thisBlock = liveVars->context->blocks[blockIndex];
        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *thisTac = tacRunner->data;
            // skip phi functions for now, just resolve other TAC operands
            if (thisTac->operation == tt_phi)
            {
                continue;
            }

            // iterate all operands
            for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
            {
                struct TACOperand *thisOperand = &thisTac->operands[operandIndex];
                switch (getUseOfOperand(thisTac, operandIndex))
                {
                case u_unused:
                    break;

                // for operands we read, set their SSA number to the relevant SSA number if possible
                case u_read:
                    struct TACOperand *mostRecentAssignment = lookupMostRecentSsaAssignment(highestSsaLiveIns, ssaLivesFromThisBlock, thisOperand);
                    if (mostRecentAssignment != NULL)
                    {
                        thisOperand->ssaNumber = mostRecentAssignment->ssaNumber;
                    }
                    break;

                // for operands we write, track that we wrote them within this block so their definitions can supersede any SSA variables live in to this block
                case u_write:
                    if (thisOperand->permutation == vp_literal)
                    {
                        ErrorAndExit(ERROR_INTERNAL, "Operand with permutation vp_literal seen in renameReadTacOperands!\n");
                    }
                    if (Set_Find(ssaLivesFromThisBlock, thisOperand) != NULL)
                    {
                        Set_Delete(ssaLivesFromThisBlock, thisOperand);
                    }
                    Set_Insert(ssaLivesFromThisBlock, thisOperand);
                    break;
                }
            }
        }

        Set_Free(highestSsaLiveIns);
        Set_Free(ssaLivesFromThisBlock);
    }
}

int compareTacLinePointers(void *lineA, void *lineB)
{
    return lineA != lineB;
}

struct LinkedListNode *removeExtraneousPhi(struct BasicBlock *block, struct LinkedListNode *extraneousPhiNode)
{
    struct LinkedListNode *next = extraneousPhiNode->next;
    struct TACLine *extraneousLine = LinkedList_Delete(block->TACList, compareTacLinePointers, extraneousPhiNode->data);
    freeTAC(extraneousLine);
    return next;
}

// this function iterates over continuous TAC lines containing one or more phi functions for the same variable
// it is capable of handling phis for join count >2, which require more than one phi function (because tt_phi reads 2 ssa operands to write a new one)
struct LinkedListNode *resolvePhisForVariable(struct Idfa *liveVars,
                                              size_t blockIndex,
                                              struct TACOperand *phiVarToResolve, // TAC operand containing the variable we are resolving phi functions for
                                              struct LinkedListNode *phiRunner,   // pointer to linked list node on which we are iterating our way through the TAC list
                                              struct Set *allLiveIns)             // set of all SSA operands for this variable
{
    struct TACOperand *resolvedPhiVar = LinkedList_PopBack(allLiveIns->elements);
    struct TACLine *phiLineToResolve = phiRunner->data;
    if (compareTacOperandIgnoreSsaNumber(phiVarToResolve, resolvedPhiVar) != 0)
    {
        ErrorAndExit(ERROR_CODE, "phiVarToResolve is not equivalent to resolvedPhiVar! (ignoring ssa number)\n");
    }

    // start off by setting the ssa number of the first read operand in the first phi function
    phiLineToResolve->operands[1].ssaNumber = resolvedPhiVar->ssaNumber;

    // track what SSA operand our last phi function assigned to
    struct TACOperand *lastPhiOutputVariable = NULL;

    // resolve arbitrarily many phi functions for as many inbound instances of the variable as exist
    // assumption: insertPhiFunctions has inserted the exact number of phi functions we need
    while (allLiveIns->elements->size > 0)
    {
        // sanity check to make sure that the correct number of phi function TAC lines got inserted by insertPhiFunctions
        if (phiRunner == NULL)
        {
            ErrorAndExit(ERROR_CODE, "Ran out of phi functions to resolve in resolvePhisForVariable!\n");
        }
        phiLineToResolve = phiRunner->data;

        if(phiLineToResolve->operation != tt_phi)
        {
            ErrorAndExit(ERROR_CODE, "Still had %zu phis to resolve but found non-phi TAC!\n", allLiveIns->elements->size);
        }

        // pop another phi var to resolve, sanity check it
        resolvedPhiVar = LinkedList_PopBack(allLiveIns->elements);

        if (compareTacOperandIgnoreSsaNumber(phiVarToResolve, resolvedPhiVar) != 0)
        {
            ErrorAndExit(ERROR_CODE, "phiVarToResolve is not equivalent to resolvedPhiVar! (ignoring ssa number)\n");
        }

        // slot our resolved var into the second "read" slot of the phi function
        phiLineToResolve->operands[2] = *resolvedPhiVar;

        // if we have already been around the loop, that means we are phi-ing a single live in variable with the output of a previous phi function within this block
        if (lastPhiOutputVariable != NULL)
        {
            phiLineToResolve->operands[1] = *lastPhiOutputVariable;
        }
        // track the operand which this phi function writes to
        lastPhiOutputVariable = &phiLineToResolve->operands[0];

        phiRunner = phiRunner->next;
    }

    return phiRunner;
}

void resolvePhiFunctions(struct Idfa *liveVars)
{
    // iterate all blocks
    for (size_t blockIndex = 0; blockIndex < liveVars->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *toResolve = liveVars->context->blocks[blockIndex];
        struct LinkedListNode *phiRunner = toResolve->TACList->head;
        // iterate all TAC in the block
        while (phiRunner != NULL)
        {
            struct TACLine *phiLineToResolve = phiRunner->data;
            if (phiLineToResolve->operation == tt_phi)
            {
                // if we encounter a phi function, its RHS operands need to be populated
                // actually copy it into this scope because removeExtraneousPhis may free the line containing the operand
                // but if removeExtraneousPhis has multiple lines to remoev it requires phiVarToResolve to still be valid
                struct TACOperand phiVarToResolve = phiLineToResolve->operands[0];

                // go out and find the set of all instances of this variable which are live into the block
                struct Set *allLiveIns = findAllSsaLiveInsForVariable(liveVars, blockIndex, &phiVarToResolve);

                if (allLiveIns->elements->size < 2)
                {
                    // if fewer than 2 differently-ssa-numbered instances of the operand exist, this phi function is extra. remove it
                    phiRunner = removeExtraneousPhi(toResolve, phiRunner);
                }
                else
                {
                    // for any number >= 2 of relevant variables live into this block, use resolvePhisForVariable to correctly resolve *all* phi functions for this variable
                    phiRunner = resolvePhisForVariable(liveVars, blockIndex, &phiVarToResolve, phiRunner, allLiveIns);
                }

                Set_Free(allLiveIns);
            }
            else
            {
                // not a phi function, skip
                phiRunner = phiRunner->next;
            }
        }
    }
}

void generateSsaForFunction(struct FunctionEntry *function)
{
    printf("Generate ssa for %s\n", function->name);

    struct IdfaContext *context = IdfaContext_Create(function->BasicBlockList);

    struct Idfa *liveVars = analyzeLiveVars(context);

    printControlFlowsAsDot(liveVars, function->name);
    Idfa_printFacts(liveVars);

    for (size_t blockIndex = 0; blockIndex < function->BasicBlockList->size; blockIndex++)
    {
        printf("%s_%zu\n", function->name, blockIndex);
        printf("gen: %zu kill: %zu in: %zu out: %zu\n",
               liveVars->facts.gen[blockIndex]->elements->size,
               liveVars->facts.kill[blockIndex]->elements->size,
               liveVars->facts.in[blockIndex]->elements->size,
               liveVars->facts.out[blockIndex]->elements->size);
    }

    insertPhiFunctions(liveVars);

    printControlFlowsAsDot(liveVars, function->name);

    renameWrittenTACOperands(context);

    printControlFlowsAsDot(liveVars, function->name);

    Idfa_Free(liveVars);
    liveVars = analyzeLiveVars(context);

    Idfa_printFacts(liveVars);
    renameReadTACOperands(liveVars);
    printControlFlowsAsDot(liveVars, function->name);

    resolvePhiFunctions(liveVars);

    printControlFlowsAsDot(liveVars, function->name);


    Idfa_Free(liveVars);
    IdfaContext_Free(context);
}

void generateSsa(struct SymbolTable *theTable)
{
    printf("generate ssa for %s\n", theTable->name);

    for (size_t entryIndex = 0; entryIndex < theTable->globalScope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = theTable->globalScope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_function:
            generateSsaForFunction(thisMember->entry);
            break;

        default:
            break;
        }
    }
}