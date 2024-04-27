#include "ssa.h"
#include "symtab.h"

#include "idfa_livevars.h"
#include "idfa_reachingdefs.h"
#include "log.h"

// TODO: implement TAC tt_declare for arguments so that we can ssa subsequent reassignments to them correctly

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

// iterate over all blocks in the order to which they are reachable from the entry block
// call operationOnBlock for each block, passing the specific block and the generic data pointer to the function
void traverseBlocksHierarchically(struct IdfaContext *context, void (*operationOnBlock)(struct BasicBlock *, void *data), void *data)
{
    // nothing to do if there are no blocks
    if (context->nBlocks == 0)
    {
        return;
    }

    struct LinkedList *blocksToTraverse = LinkedList_New();

    // block 0 is always the entry of the function, so as long as we start with it we will be fine
    LinkedList_Append(blocksToTraverse, context->blocks[0]);
    struct Set *visited = Set_New(ssizet_compare, NULL);

    struct Set *stronglyConnectedComponent = Set_New(ssizet_compare, NULL);

    while (blocksToTraverse->size > 0)
    {
        // grab the block at the front of the queue
        struct BasicBlock *thisBlock = LinkedList_PopFront(blocksToTraverse);

        // figure out if we have visited all predecessors of this block
        u8 sawAllPredecessors = 1;
        for (struct LinkedListNode *predRunner = context->predecessors[thisBlock->labelNum]->elements->head; predRunner != NULL; predRunner = predRunner->next)
        {
            struct BasicBlock *predecessorBlock = predRunner->data;

            if (Set_Find(visited, predecessorBlock) == NULL)
            {
                sawAllPredecessors = 0;
                break;
            }
        }

        // if we have not yet visited all predecessors of this block, put it back on the list to be traversed later
        if (!sawAllPredecessors)
        {
            // if the strongly connected component is the entire blocksToTraverseList, forcibly override to visit this block next loop
            if (stronglyConnectedComponent->elements->size != blocksToTraverse->size)
            {
                Set_Insert(stronglyConnectedComponent, thisBlock);
                LinkedList_Append(blocksToTraverse, thisBlock);
                continue;
            }
        }
        operationOnBlock(thisBlock, data);

        // if we successfully visited a block, we may have satisfied a predecessor requirement to visit some other block
        // thus, we should always clear the strongly connected component whenever we visit
        Set_Clear(stronglyConnectedComponent);

        // mark this block as visited
        Set_Insert(visited, thisBlock);

        for (struct LinkedListNode *successorRunner = context->successors[thisBlock->labelNum]->elements->head; successorRunner != NULL; successorRunner = successorRunner->next)
        {
            struct BasicBlock *successorBlock = successorRunner->data;
            if (Set_Find(visited, successorBlock) == NULL)
            {
                LinkedList_Append(blocksToTraverse, successorBlock);
            }
        }
    }

    LinkedList_Free(blocksToTraverse, NULL);
    Set_Free(visited);
    Set_Free(stronglyConnectedComponent);
}

struct PhiContext
{
    struct Idfa *reachingDefs;
    struct Set *ssaNumbers;
};

void insertPhiFunctionsForBlock(struct BasicBlock *block, void *data)
{
    struct AST fakePhiTree;
    memset(&fakePhiTree, 0, sizeof(struct AST));

    struct PhiContext *context = data;

    struct Idfa *reachingDefs = context->reachingDefs;

    size_t blockEntryTacIndex = 0;
    if (block->TACList->size > 0)
    {
        blockEntryTacIndex = ((struct TACLine *)block->TACList->head->data)->index;
    }
    // hash table to map from TAC operand -> count of number of predecessor blocks the variable is live out from
    struct HashTable *phiVars = HashTable_New(1, hashTacOperand, TACOperand_CompareIgnoreSsaNumber, NULL, (void (*)(void *))Set_Free);
    // iterate all predecessor blocks
    for (struct LinkedListNode *predecessorRunner = reachingDefs->context->predecessors[block->labelNum]->elements->head; predecessorRunner != NULL; predecessorRunner = predecessorRunner->next)
    {
        struct BasicBlock *predecessorBlock = predecessorRunner->data;
        // iterate all live vars out facts from the predecessor
        for (struct LinkedListNode *liveVarRunner = reachingDefs->facts.out[predecessorBlock->labelNum]->elements->head; liveVarRunner != NULL; liveVarRunner = liveVarRunner->next)
        {
            struct TACOperand *liveOut = liveVarRunner->data;

            struct Set *ssasLiveOut = HashTable_Lookup(phiVars, liveOut);
            if (ssasLiveOut == NULL)
            {
                ssasLiveOut = Set_New(TACOperand_Compare, NULL);
                HashTable_Insert(phiVars, liveOut, ssasLiveOut);
            }

            Set_Insert(ssasLiveOut, liveOut);
        }
    }

    // iterate all entries in the hash table we created in the loop above
    for (struct LinkedListNode *phiVarRunner = phiVars->buckets[0]->elements->head; phiVarRunner != NULL; phiVarRunner = phiVarRunner->next)
    {
        struct HashTableEntry *phiVarEntry = phiVarRunner->data;
        struct Set *inboundSsasToPhi = phiVarEntry->value;
        // for any variables which are live out of more than one predecessor block
        if (inboundSsasToPhi->elements->size > 1)
        {
            // predecrement so we can insert exactly the right number of phis
            struct TACOperand *phiVar = phiVarEntry->key;

            // insert a phi for each occurrence (- 1 because the first phi can take 2 unique operands instead of one new operand and the output of a previous phi)
            while (inboundSsasToPhi->elements->size > 1)
            {
                struct TACLine *newPhi = newTACLine(tt_phi, &fakePhiTree);
                newPhi->index = blockEntryTacIndex;
                struct TACOperand *assignedSsa = ssaOperandLookupOrInsert(context->ssaNumbers, phiVar);
                newPhi->operands[0] = *assignedSsa;
                assignedSsa->ssaNumber++;

                struct TACOperand *liveIn = LinkedList_PopFront(inboundSsasToPhi->elements);
                newPhi->operands[1] = *liveIn;
                liveIn = LinkedList_PopFront(inboundSsasToPhi->elements);
                newPhi->operands[2] = *liveIn;
                BasicBlock_prepend(block, newPhi);

                Set_Insert(inboundSsasToPhi, &newPhi->operands[0]);
            }
        }
    }

    HashTable_Free(phiVars);

    Idfa_Redo(reachingDefs);
}

void insertPhiFunctions(struct Idfa *reachingDefs, struct Set *ssaNumbers)
{
    struct PhiContext context = {reachingDefs, ssaNumbers};
    traverseBlocksHierarchically(reachingDefs->context, insertPhiFunctionsForBlock, &context);
}

// rename all operands which are written to in the block to have unique SSA numbers
void renameWrittenTacOperandsForBlock(struct BasicBlock *block, void *data)
{
    // set of all operands which are ever written in the IdfaContext
    struct Set *ssaOperands = data;

    // iterate all TAC in the block
    for (struct LinkedListNode *tacRunner = block->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
    {
        struct TACLine *thisTac = tacRunner->data;
        // iterate all operands in the TAC
        for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
        {
            // written operands are the only ones which need to be assigned an SSA number at this point
            if (getUseOfOperand(thisTac, operandIndex) == u_write)
            {
                struct TACOperand *thisOperand = &thisTac->operands[operandIndex];

                // assign a unique SSA number to the operand
                struct TACOperand *ssaOperand = ssaOperandLookupOrInsert(ssaOperands, thisOperand);
                thisOperand->ssaNumber = ssaOperand->ssaNumber++;
            }
        }
    }
}

// go over all TAC operands which are assigned to and give them unique SSA numbers
struct Set *renameWrittenTACOperands(struct IdfaContext *context)
{
    struct Set *ssaOperandNumbers = Set_New(TACOperand_CompareIgnoreSsaNumber, free);

    traverseBlocksHierarchically(context, renameWrittenTacOperandsForBlock, ssaOperandNumbers);

    return ssaOperandNumbers;
}

struct Set *findHighestSsaLiveIns(struct Idfa *liveVars, size_t blockIndex)
{
    struct Set *highestSsaLiveIns = Set_New(TACOperand_CompareIgnoreSsaNumber, NULL);

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
struct Set *findUnusedSsaReachingDefs(struct Idfa *reachingDefs, size_t blockIndex, struct TACOperand *operand)
{
    struct Set *matchingOperands = Set_New(TACOperand_Compare, NULL);
    for (struct LinkedListNode *reachingDefRunner = reachingDefs->facts.in[blockIndex]->elements->head; reachingDefRunner != NULL; reachingDefRunner = reachingDefRunner->next)
    {
        struct TACOperand *reachingDef = reachingDefRunner->data;
        if (TACOperand_CompareIgnoreSsaNumber(operand, reachingDef) == 0)
        {
            // only include operands which are not killed in the block
            if (Set_Find(reachingDefs->facts.kill[blockIndex], reachingDef) == NULL)
            {
                Set_Insert(matchingOperands, reachingDef);
            }
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

void renameReadTACOperandsInBlock(struct BasicBlock *block, void *data)
{
    struct Idfa *liveVars = data;

    // the highest SSA numbers that live in to this basic block
    struct Set *highestSsaLiveIns = findHighestSsaLiveIns(liveVars, block->labelNum);
    // any SSA operands which are assigned to within the block
    struct Set *ssaLivesFromThisBlock = Set_New(TACOperand_CompareIgnoreSsaNumber, NULL);

    // iterate all TAC in the block
    struct BasicBlock *thisBlock = liveVars->context->blocks[block->labelNum];
    for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
    {
        struct TACLine *thisTac = tacRunner->data;

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
                // don't modify SSA numbers if it is being read by a phi function
                if (thisTac->operation == tt_phi)
                {
                    break;
                }
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
                    InternalError("Written operand with permutation vp_literal seen in renameReadTacOperands!");
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

void renameReadTACOperands(struct Idfa *liveVars)
{
    traverseBlocksHierarchically(liveVars->context, renameReadTACOperandsInBlock, liveVars);
}

void doFunChecks(struct IdfaContext *context)
{
    if (context->nBlocks == 0)
    {
        return;
    }

    struct Idfa *reachingDefs = analyzeReachingDefs(context);

    struct BasicBlock *lastBlock = NULL;
    for (size_t blockIndex = 0; blockIndex < context->nBlocks; blockIndex++)
    {
        if (context->successors[blockIndex]->elements->size == 0)
        {
            lastBlock = context->blocks[blockIndex];
            continue;
        }
    }

    printf("%p%p\n", lastBlock, reachingDefs);
    printf("Potentially unused variables: \t");

    struct Set *reachingOut = reachingDefs->facts.out[lastBlock->labelNum];
    for (struct LinkedListNode *runner = reachingOut->elements->head; runner != NULL; runner = runner->next)
    {
        struct TACOperand *liveOutOperand = runner->data;
        if (liveOutOperand->permutation == vp_standard)
        {
            printTACOperand(liveOutOperand);
            printf(" ");
        }
    }

    Idfa_Free(reachingDefs);
}

void generateSsaForFunction(struct FunctionEntry *function)
{
    struct IdfaContext *context = IdfaContext_Create(function->BasicBlockList);

    struct Set *ssaNumbers = renameWrittenTACOperands(context);

    struct Idfa *reachingDefs = analyzeReachingDefs(context);
    insertPhiFunctions(reachingDefs, ssaNumbers);

    Set_Free(ssaNumbers);
    Idfa_Redo(reachingDefs);

    renameReadTACOperands(reachingDefs);
    Idfa_Free(reachingDefs);

    // doFunChecks(context);

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