#include "ssa.h"
#include "symtab.h"

#include "idfa_livevars.h"
#include "idfa_reachingdefs.h"
#include "log.h"

// TODO: implement TAC tt_declare for arguments so that we can ssa subsequent reassignments to them correctly

size_t hash_tac_operand(void *operand)
{
    size_t hash = 0;
    for (size_t byteIndex = 0; byteIndex < sizeof(struct TACOperand); byteIndex++)
    {
        hash += ((u8 *)operand)[byteIndex];
        hash <<= 1;
    }
    return hash;
}

void print_control_flows_as_dot(struct Idfa *idfa, char *functionName, FILE *outFile)
{
    fprintf(outFile, "digraph %s{\nedge[dir=forward]\nnode[shape=plaintext,style=filled]\n", functionName);
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        for (struct LinkedListNode *flowRunner = idfa->context->successors[blockIndex]->elements->head; flowRunner != NULL; flowRunner = flowRunner->next)
        {
            struct BasicBlock *destinationBlock = flowRunner->data;
            fprintf(outFile, "%s_%zu:s->%s_%zu:n\n", functionName, blockIndex, functionName, destinationBlock->labelNum);
        }

        struct BasicBlock *thisBlock = idfa->context->blocks[blockIndex];
        fprintf(outFile, "%s_%zu[label=<%s_%zu<BR />\n", functionName, thisBlock->labelNum, functionName, thisBlock->labelNum);

        for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            char *tacString = sprint_tac_line(tacRunner->data);
            fprintf(outFile, "%s<BR />\n", tacString);
            free(tacString);
        }
        fprintf(outFile, ">]\n");
    }

    fprintf(outFile, "}\n\n\n");
}

struct TACOperand *ssa_operand_lookup_or_insert(struct Set *ssaOperands, struct TACOperand *originalOperand)
{
    struct TACOperand *foundOperand = set_find(ssaOperands, originalOperand);
    if (foundOperand == NULL)
    {
        struct TACOperand *newOperand = malloc(sizeof(struct TACOperand));
        memcpy(newOperand, originalOperand, sizeof(struct TACOperand));
        newOperand->ssaNumber = 0;
        set_insert(ssaOperands, newOperand);
        return newOperand;
    }
    return foundOperand;
}

// iterate over all blocks in the order to which they are reachable from the entry block
// call operationOnBlock for each block, passing the specific block and the generic data pointer to the function
void traverse_blocks_hierarchically(struct IdfaContext *context, void (*operationOnBlock)(struct BasicBlock *, void *data), void *data)
{
    // nothing to do if there are no blocks
    if (context->nBlocks == 0)
    {
        return;
    }

    struct LinkedList *blocksToTraverse = linked_list_new();

    // block 0 is always the entry of the function, so as long as we start with it we will be fine
    linked_list_append(blocksToTraverse, context->blocks[0]);
    struct Set *visited = set_new(ssizet_compare, NULL);

    struct Set *stronglyConnectedComponent = set_new(ssizet_compare, NULL);

    while (blocksToTraverse->size > 0)
    {
        // grab the block at the front of the queue
        struct BasicBlock *thisBlock = linked_list_pop_front(blocksToTraverse);

        // figure out if we have visited all predecessors of this block
        u8 sawAllPredecessors = 1;
        for (struct LinkedListNode *predRunner = context->predecessors[thisBlock->labelNum]->elements->head; predRunner != NULL; predRunner = predRunner->next)
        {
            struct BasicBlock *predecessorBlock = predRunner->data;

            if (set_find(visited, predecessorBlock) == NULL)
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
                set_insert(stronglyConnectedComponent, thisBlock);
                linked_list_append(blocksToTraverse, thisBlock);
                continue;
            }
        }
        operationOnBlock(thisBlock, data);

        // if we successfully visited a block, we may have satisfied a predecessor requirement to visit some other block
        // thus, we should always clear the strongly connected component whenever we visit
        set_clear(stronglyConnectedComponent);

        // mark this block as visited
        set_insert(visited, thisBlock);

        for (struct LinkedListNode *successorRunner = context->successors[thisBlock->labelNum]->elements->head; successorRunner != NULL; successorRunner = successorRunner->next)
        {
            struct BasicBlock *successorBlock = successorRunner->data;
            if (set_find(visited, successorBlock) == NULL)
            {
                linked_list_append(blocksToTraverse, successorBlock);
            }
        }
    }

    linked_list_free(blocksToTraverse, NULL);
    set_free(visited);
    set_free(stronglyConnectedComponent);
}

struct PhiContext
{
    struct Idfa *reachingDefs;
    struct Set *ssaNumbers;
};

void insert_phi_functions_for_block(struct BasicBlock *block, void *data)
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
    struct HashTable *phiVars = hash_table_new(1, hash_tac_operand, tac_operand_compare_ignore_ssa_number, NULL, (void (*)(void *))set_free);
    // iterate all predecessor blocks
    for (struct LinkedListNode *predecessorRunner = reachingDefs->context->predecessors[block->labelNum]->elements->head; predecessorRunner != NULL; predecessorRunner = predecessorRunner->next)
    {
        struct BasicBlock *predecessorBlock = predecessorRunner->data;
        // iterate all live vars out facts from the predecessor
        for (struct LinkedListNode *liveVarRunner = reachingDefs->facts.out[predecessorBlock->labelNum]->elements->head; liveVarRunner != NULL; liveVarRunner = liveVarRunner->next)
        {
            struct TACOperand *liveOut = liveVarRunner->data;

            struct Set *ssasLiveOut = hash_table_lookup(phiVars, liveOut);
            if (ssasLiveOut == NULL)
            {
                ssasLiveOut = set_new(tac_operand_compare, NULL);
                hash_table_insert(phiVars, liveOut, ssasLiveOut);
            }

            set_insert(ssasLiveOut, liveOut);
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
                struct TACLine *newPhi = new_tac_line(TT_PHI, &fakePhiTree);
                newPhi->index = blockEntryTacIndex;
                struct TACOperand *assignedSsa = ssa_operand_lookup_or_insert(context->ssaNumbers, phiVar);
                newPhi->operands[0] = *assignedSsa;
                assignedSsa->ssaNumber++;

                struct TACOperand *liveIn = linked_list_pop_front(inboundSsasToPhi->elements);
                newPhi->operands[1] = *liveIn;
                liveIn = linked_list_pop_front(inboundSsasToPhi->elements);
                newPhi->operands[2] = *liveIn;
                basic_block_prepend(block, newPhi);

                set_insert(inboundSsasToPhi, &newPhi->operands[0]);
            }
        }
    }

    hash_table_free(phiVars);

    idfa_redo(reachingDefs);
}

void insert_phi_functions(struct Idfa *reachingDefs, struct Set *ssaNumbers)
{
    struct PhiContext context = {reachingDefs, ssaNumbers};
    traverse_blocks_hierarchically(reachingDefs->context, insert_phi_functions_for_block, &context);
}

// rename all operands which are written to in the block to have unique SSA numbers
void rename_written_tac_operands_for_block(struct BasicBlock *block, void *data)
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
            if (get_use_of_operand(thisTac, operandIndex) == U_WRITE)
            {
                struct TACOperand *thisOperand = &thisTac->operands[operandIndex];

                // assign a unique SSA number to the operand
                struct TACOperand *ssaOperand = ssa_operand_lookup_or_insert(ssaOperands, thisOperand);
                thisOperand->ssaNumber = ssaOperand->ssaNumber++;
            }
        }
    }
}

// go over all TAC operands which are assigned to and give them unique SSA numbers
struct Set *rename_written_tac_operands(struct IdfaContext *context)
{
    struct Set *ssaOperandNumbers = set_new(tac_operand_compare_ignore_ssa_number, free);

    traverse_blocks_hierarchically(context, rename_written_tac_operands_for_block, ssaOperandNumbers);

    return ssaOperandNumbers;
}

struct Set *find_highest_ssa_live_ins(struct Idfa *liveVars, size_t blockIndex)
{
    struct Set *highestSsaLiveIns = set_new(tac_operand_compare_ignore_ssa_number, NULL);

    for (struct LinkedListNode *liveInRunner = liveVars->facts.in[blockIndex]->elements->head; liveInRunner != NULL; liveInRunner = liveInRunner->next)
    {
        struct TACOperand *thisLiveIn = liveInRunner->data;
        struct TACOperand *highestLiveIn = set_find(highestSsaLiveIns, thisLiveIn);
        if (highestLiveIn == NULL)
        {
            set_insert(highestSsaLiveIns, thisLiveIn);
        }
        else if (highestLiveIn->ssaNumber < thisLiveIn->ssaNumber)
        {
            set_delete(highestSsaLiveIns, highestLiveIn);
            set_insert(highestSsaLiveIns, thisLiveIn);
        }
    }

    return highestSsaLiveIns;
}

// find all SSA variables live in to block blockIndex based on tacOperand
struct Set *find_unused_ssa_reaching_defs(struct Idfa *reachingDefs, size_t blockIndex, struct TACOperand *operand)
{
    struct Set *matchingOperands = set_new(tac_operand_compare, NULL);
    for (struct LinkedListNode *reachingDefRunner = reachingDefs->facts.in[blockIndex]->elements->head; reachingDefRunner != NULL; reachingDefRunner = reachingDefRunner->next)
    {
        struct TACOperand *reachingDef = reachingDefRunner->data;
        if (tac_operand_compare_ignore_ssa_number(operand, reachingDef) == 0)
        {
            // only include operands which are not killed in the block
            if (set_find(reachingDefs->facts.kill[blockIndex], reachingDef) == NULL)
            {
                set_insert(matchingOperands, reachingDef);
            }
        }
    }
    return matchingOperands;
}

struct TACOperand *lookup_most_recent_ssa_assignment(struct Set *highestSsaLiveIns, struct Set *assignmentsThisBlock, struct TACOperand *originalOperand)
{
    struct TACOperand *mostRecentAssignment = NULL;
    // first, attempt to find an assignment from within the current block
    struct TACOperand *assignmentInBlock = set_find(assignmentsThisBlock, originalOperand);
    if (assignmentInBlock == NULL)
    {
        // no assignment within the block, so search from the set of live SSA's coming in to the block
        mostRecentAssignment = set_find(highestSsaLiveIns, originalOperand);
    }
    else
    {
        mostRecentAssignment = assignmentInBlock;
    }
    return mostRecentAssignment;
}

void rename_read_tac_operands_in_block(struct BasicBlock *block, void *data)
{
    struct Idfa *liveVars = data;

    // the highest SSA numbers that live in to this basic block
    struct Set *highestSsaLiveIns = find_highest_ssa_live_ins(liveVars, block->labelNum);
    // any SSA operands which are assigned to within the block
    struct Set *ssaLivesFromThisBlock = set_new(tac_operand_compare_ignore_ssa_number, NULL);

    // iterate all TAC in the block
    struct BasicBlock *thisBlock = liveVars->context->blocks[block->labelNum];
    for (struct LinkedListNode *tacRunner = thisBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
    {
        struct TACLine *thisTac = tacRunner->data;

        // iterate all operands
        for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
        {
            struct TACOperand *thisOperand = &thisTac->operands[operandIndex];
            switch (get_use_of_operand(thisTac, operandIndex))
            {
            case U_UNUSED:
                break;

            // for operands we read, set their SSA number to the relevant SSA number if possible
            case U_READ:
                // don't modify SSA numbers if it is being read by a phi function
                if (thisTac->operation == TT_PHI)
                {
                    break;
                }
                struct TACOperand *mostRecentAssignment = lookup_most_recent_ssa_assignment(highestSsaLiveIns, ssaLivesFromThisBlock, thisOperand);
                if (mostRecentAssignment != NULL)
                {
                    thisOperand->ssaNumber = mostRecentAssignment->ssaNumber;
                }
                break;

            // for operands we write, track that we wrote them within this block so their definitions can supersede any SSA variables live in to this block
            case U_WRITE:
                if (thisOperand->permutation == VP_LITERAL)
                {
                    InternalError("Written operand with permutation VP_LITERAL seen in renameReadTacOperands!");
                }
                if (set_find(ssaLivesFromThisBlock, thisOperand) != NULL)
                {
                    set_delete(ssaLivesFromThisBlock, thisOperand);
                }
                set_insert(ssaLivesFromThisBlock, thisOperand);
                break;
            }
        }
    }

    set_free(highestSsaLiveIns);
    set_free(ssaLivesFromThisBlock);
}

void rename_read_tac_operands(struct Idfa *liveVars)
{
    traverse_blocks_hierarchically(liveVars->context, rename_read_tac_operands_in_block, liveVars);
}

void do_fun_checks(struct IdfaContext *context)
{
    if (context->nBlocks == 0)
    {
        return;
    }

    struct Idfa *reachingDefs = analyze_reaching_defs(context);

    // TODO: warn for unused variables
    /*
    struct BasicBlock *lastBlock = NULL;
    for (size_t blockIndex = 0; blockIndex < context->nBlocks; blockIndex++)
    {
        if (context->successors[blockIndex]->elements->size == 0)
        {
            lastBlock = context->blocks[blockIndex];
            continue;
        }
    }

    log(LOG_WARNING, "Potentially unused variables: \t");

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
    */

    idfa_free(reachingDefs);
}

void generate_ssa_for_function(struct FunctionEntry *function)
{
    struct IdfaContext *context = idfa_context_create(function->BasicBlockList);

    struct Set *ssaNumbers = rename_written_tac_operands(context);

    struct Idfa *reachingDefs = analyze_reaching_defs(context);
    insert_phi_functions(reachingDefs, ssaNumbers);

    set_free(ssaNumbers);
    idfa_redo(reachingDefs);

    rename_read_tac_operands(reachingDefs);
    idfa_free(reachingDefs);

    // doFunChecks(context);

    idfa_context_free(context);
}

void generate_ssa(struct SymbolTable *theTable)
{
    log(LOG_INFO, "Generate ssa for %s", theTable->name);

    for (size_t entryIndex = 0; entryIndex < theTable->globalScope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = theTable->globalScope->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case E_FUNCTION:
            generate_ssa_for_function(thisMember->entry);
            break;

        default:
            break;
        }
    }
}