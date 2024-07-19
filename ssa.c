#include "ssa.h"
#include "symtab.h"

#include "idfa_livevars.h"
#include "idfa_reachingdefs.h"
#include "log.h"

#include "mbcl/hash_table.h"

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
    for (size_t blockIndex = 0; blockIndex < idfa->context->blocks->size; blockIndex++)
    {
        Iterator *flowRunner = NULL;
        for (flowRunner = set_begin(array_at(idfa->context->successors, blockIndex)); iterator_gettable(flowRunner); iterator_next(flowRunner))
        {
            struct BasicBlock *destinationBlock = iterator_get(flowRunner);
            fprintf(outFile, "%s_%zu:s->%s_%zu:n\n", functionName, blockIndex, functionName, destinationBlock->labelNum);
        }
        iterator_free(flowRunner);

        struct BasicBlock *thisBlock = array_at(idfa->context->blocks, blockIndex);
        fprintf(outFile, "%s_%zu[label=<%s_%zu<BR />\n", functionName, thisBlock->labelNum, functionName, thisBlock->labelNum);

        Iterator *tacRunner = NULL;
        for (tacRunner = list_begin(thisBlock->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
        {
            char *tacString = sprint_tac_line(iterator_get(tacRunner));
            fprintf(outFile, "%s<BR />\n", tacString);
            free(tacString);
        }
        iterator_free(tacRunner);
        fprintf(outFile, ">]\n");
    }

    fprintf(outFile, "}\n\n\n");
}

struct TACOperand *ssa_operand_lookup_or_insert(List *ssaOperands, struct TACOperand *originalOperand)
{
    struct TACOperand *foundOperand = list_find(ssaOperands, originalOperand);
    if (foundOperand == NULL)
    {
        struct TACOperand *newOperand = malloc(sizeof(struct TACOperand));
        memcpy(newOperand, originalOperand, sizeof(struct TACOperand));
        newOperand->ssaNumber = 0;
        list_append(ssaOperands, newOperand);
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

    List *blocksToTraverse = list_new(NULL, NULL);

    // block 0 is always the entry of the function, so as long as we start with it we will be fine
    list_append(blocksToTraverse, array_at(context->blocks, 0));
    Set *visited = set_new(NULL, ssizet_compare);

    Set *stronglyConnectedComponent = set_new(NULL, ssizet_compare);

    while (blocksToTraverse->size > 0)
    {
        // grab the block at the front of the queue
        struct BasicBlock *thisBlock = list_pop_front(blocksToTraverse);

        // figure out if we have visited all predecessors of this block
        u8 sawAllPredecessors = 1;
        Iterator *predRunner = NULL;
        for (predRunner = set_begin(array_at(context->predecessors, thisBlock->labelNum)); iterator_gettable(predRunner); iterator_next(predRunner))
        {
            struct BasicBlock *predecessorBlock = iterator_get(predRunner);

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
            if (stronglyConnectedComponent->size != blocksToTraverse->size)
            {
                set_insert(stronglyConnectedComponent, thisBlock);
                list_append(blocksToTraverse, thisBlock);
                continue;
            }
        }
        operationOnBlock(thisBlock, data);

        // if we successfully visited a block, we may have satisfied a predecessor requirement to visit some other block
        // thus, we should always clear the strongly connected component whenever we visit
        set_clear(stronglyConnectedComponent);

        // mark this block as visited if it hasn't been already
        set_try_insert(visited, thisBlock);

        Iterator *successorRunner = NULL;
        for (successorRunner = set_begin(array_at(context->successors, thisBlock->labelNum)); iterator_gettable(successorRunner); iterator_next(successorRunner))
        {
            struct BasicBlock *successorBlock = iterator_get(successorRunner);
            if (set_find(visited, successorBlock) == NULL)
            {
                list_append(blocksToTraverse, successorBlock);
            }
        }
    }

    list_free(blocksToTraverse);
    set_free(visited);
    set_free(stronglyConnectedComponent);
}

struct PhiContext
{
    struct Idfa *reachingDefs;
    List *ssaNumbers;
};

void insert_phi_functions_for_block(struct BasicBlock *block, void *data)
{
    struct Ast fakePhiTree;
    memset(&fakePhiTree, 0, sizeof(struct Ast));

    struct PhiContext *context = data;

    struct Idfa *reachingDefs = context->reachingDefs;

    size_t blockEntryTacIndex = 0;
    if (block->TACList->size > 0)
    {
        blockEntryTacIndex = ((struct TACLine *)block->TACList->head->data)->index;
    }

    // hash table to map from TAC operand -> count of number of predecessor blocks the variable is live out from
    // struct HashTable *phiVars = hash_table_new(1, hash_tac_operand, tac_operand_compare_ignore_ssa_number, NULL, (void (*)(void *))set_free);
    HashTable *phiVars = hash_table_new(NULL, (void (*)(void *))set_free, tac_operand_compare_ignore_ssa_number, hash_tac_operand, block->TACList->size + 1);
    // iterate all predecessor blocks
    Iterator *predecessorRunner = NULL;
    for (predecessorRunner = set_begin(array_at(reachingDefs->context->predecessors, block->labelNum)); iterator_gettable(predecessorRunner); iterator_next(predecessorRunner))
    {
        struct BasicBlock *predecessorBlock = iterator_get(predecessorRunner);
        // iterate all live vars out facts from the predecessor
        Iterator *liveVarRunner = NULL;
        for (liveVarRunner = set_begin(array_at(reachingDefs->facts.out, predecessorBlock->labelNum)); iterator_gettable(liveVarRunner); iterator_next(liveVarRunner))
        {
            struct TACOperand *liveOut = iterator_get(liveVarRunner);

            Set *ssasLiveOut = hash_table_find(phiVars, liveOut);
            if (ssasLiveOut == NULL)
            {
                ssasLiveOut = set_new(NULL, tac_operand_compare);
                hash_table_insert(phiVars, liveOut, ssasLiveOut);
            }

            set_try_insert(ssasLiveOut, liveOut);
        }
        iterator_free(liveVarRunner);
    }
    iterator_free(predecessorRunner);

    // TODO: implement hash table stuff
    // iterate all entries in the hash table we created in the loop above
    Iterator *phiVarRunner = NULL;
    for (phiVarRunner = hash_table_begin(phiVars); iterator_gettable(phiVarRunner); iterator_next(phiVarRunner))
    {
        HashTableEntry *phiVarEntry = iterator_get(phiVarRunner);
        struct TACOperand *phiVar = phiVarEntry->key;
        Set *inboundSsasSet = phiVarEntry->value;
        Deque *inboundSsasToPhi = deque_new(NULL);

        Iterator *inboundSsaIterator = NULL;
        for(inboundSsaIterator = set_begin(inboundSsasSet); iterator_gettable(inboundSsaIterator); iterator_next(inboundSsaIterator))
        {
            deque_push_back(inboundSsasToPhi, iterator_get(inboundSsaIterator));
        }
        iterator_free(inboundSsaIterator);


        // for any variables which are live out of more than one predecessor block
        if (inboundSsasToPhi->size > 1)
        {
            // predecrement so we can insert exactly the right number of phis

            // insert a phi for each occurrence (- 1 because the first phi can take 2 unique operands instead of one new operand and the output of a previous phi)
            while (inboundSsasToPhi->size > 1)
            {
                struct TACLine *newPhi = new_tac_line(TT_PHI, &fakePhiTree);
                newPhi->index = blockEntryTacIndex;
                struct TACOperand *assignedSsa = ssa_operand_lookup_or_insert(context->ssaNumbers, phiVar);
                newPhi->operands[0] = *assignedSsa;
                assignedSsa->ssaNumber++;

                struct TACOperand *liveIn = deque_pop_front(inboundSsasToPhi);
                newPhi->operands[1] = *liveIn;
                liveIn = deque_pop_front(inboundSsasToPhi);
                newPhi->operands[2] = *liveIn;
                basic_block_prepend(block, newPhi);

                deque_push_back(inboundSsasToPhi, &newPhi->operands[0]);
            }
        }
    }

    hash_table_free(phiVars);

    idfa_redo(reachingDefs);
}

void insert_phi_functions(struct Idfa *reachingDefs, List *ssaNumbers)
{
    struct PhiContext context = {reachingDefs, ssaNumbers};
    traverse_blocks_hierarchically(reachingDefs->context, insert_phi_functions_for_block, &context);
}

// rename all operands which are written to in the block to have unique SSA numbers
void rename_written_tac_operands_for_block(struct BasicBlock *block, void *data)
{
    // set of all operands which are ever written in the IdfaContext
    List *ssaOperands = data;

    // iterate all TAC in the block
    Iterator *tacRunner = NULL;
    for (tacRunner = list_begin(block->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
    {
        struct TACLine *thisTac = iterator_get(tacRunner);
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
    iterator_free(tacRunner);
}

// go over all TAC operands which are assigned to and give them unique SSA numbers
List *rename_written_tac_operands(struct IdfaContext *context)
{
    List *ssaOperandNumbers = list_new(free, tac_operand_compare_ignore_ssa_number);

    traverse_blocks_hierarchically(context, rename_written_tac_operands_for_block, ssaOperandNumbers);

    return ssaOperandNumbers;
}

Set *find_highest_ssa_live_ins(struct Idfa *liveVars, size_t blockIndex)
{
    Set *highestSsaLiveIns = set_new(NULL, tac_operand_compare_ignore_ssa_number);

    Iterator *liveInRunner = NULL;
    for (liveInRunner = set_begin(array_at(liveVars->facts.in, blockIndex)); iterator_gettable(liveInRunner); iterator_next(liveInRunner))
    {
        struct TACOperand *thisLiveIn = iterator_get(liveInRunner);
        struct TACOperand *highestLiveIn = set_find(highestSsaLiveIns, thisLiveIn);
        if (highestLiveIn == NULL)
        {
            set_insert(highestSsaLiveIns, thisLiveIn);
        }
        else if (highestLiveIn->ssaNumber < thisLiveIn->ssaNumber)
        {
            set_remove(highestSsaLiveIns, highestLiveIn);
            set_insert(highestSsaLiveIns, thisLiveIn);
        }
    }
    iterator_free(liveInRunner);

    return highestSsaLiveIns;
}

// find all SSA variables live in to block blockIndex based on tacOperand
Set *find_unused_ssa_reaching_defs(struct Idfa *reachingDefs, size_t blockIndex, struct TACOperand *operand)
{
    Set *matchingOperands = set_new(NULL, tac_operand_compare);

    Iterator *reachingDefRunner = NULL;
    for (reachingDefRunner = set_begin(array_at(reachingDefs->facts.in, blockIndex)); iterator_gettable(reachingDefRunner); iterator_next(reachingDefRunner))
    {
        struct TACOperand *reachingDef = iterator_get(reachingDefRunner);
        if (tac_operand_compare_ignore_ssa_number(operand, reachingDef) == 0)
        {
            // only include operands which are not killed in the block
            if (set_find(array_at(reachingDefs->facts.kill, blockIndex), reachingDef) == NULL)
            {
                set_insert(matchingOperands, reachingDef);
            }
        }
    }
    return matchingOperands;
}

struct TACOperand *lookup_most_recent_ssa_assignment(Set *highestSsaLiveIns, Set *assignmentsThisBlock, struct TACOperand *originalOperand)
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
    Set *highestSsaLiveIns = find_highest_ssa_live_ins(liveVars, block->labelNum);
    // any SSA operands which are assigned to within the block
    Set *ssaLivesFromThisBlock = set_new(NULL, tac_operand_compare_ignore_ssa_number);

    // iterate all TAC in the block
    Iterator *tacRunner = NULL;
    for (tacRunner = list_begin(block->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
    {
        struct TACLine *thisTac = iterator_get(tacRunner);

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
                if ((thisOperand->permutation == VP_LITERAL_STR) || (thisOperand->permutation == VP_LITERAL_VAL))
                {
                    InternalError("Written operand with permutation VP_LITERAL_STR or VP_LITERAL_VAL seen in renameReadTacOperands!");
                }
                if (set_find(ssaLivesFromThisBlock, thisOperand) != NULL)
                {
                    set_remove(ssaLivesFromThisBlock, thisOperand);
                }
                set_insert(ssaLivesFromThisBlock, thisOperand);
                break;
            }
        }
    }
    iterator_free(tacRunner);

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

    Set *reachingOut = reachingDefs->facts.out[lastBlock->labelNum];
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

    List *ssaNumbers = rename_written_tac_operands(context);

    struct Idfa *reachingDefs = analyze_reaching_defs(context);
    insert_phi_functions(reachingDefs, ssaNumbers);

    list_free(ssaNumbers);
    idfa_redo(reachingDefs);

    rename_read_tac_operands(reachingDefs);
    idfa_free(reachingDefs);

    // doFunChecks(context);

    idfa_context_free(context);
}

void generate_ssa(struct SymbolTable *theTable)
{
    log(LOG_INFO, "Generate ssa for %s", theTable->name);

    Iterator *entryIterator = NULL;
    for (entryIterator = set_begin(theTable->globalScope->entries); iterator_gettable(entryIterator); iterator_next(entryIterator))
    {
        struct ScopeMember *thisMember = iterator_get(entryIterator);
        switch (thisMember->type)
        {
        case E_FUNCTION:
            generate_ssa_for_function(thisMember->entry);
            break;

        default:
            break;
        }
    }
    iterator_free(entryIterator);
}