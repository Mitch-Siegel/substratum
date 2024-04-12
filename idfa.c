#include "idfa.h"
#include "symtab_basicblock.h"
#include "util.h"

// iterative dataflow analysis
int compareBasicBlocks(void *blockA, void *blockB)
{
    return blockA != blockB;
}

struct Set **generateSuccessors(struct BasicBlock **blocks, size_t nBlocks)
{
    struct Set **blockSuccessors = malloc(nBlocks * sizeof(struct LinkedList *));

    for (size_t blockIndex = 0; blockIndex < nBlocks; blockIndex++)
    {
        blockSuccessors[blockIndex] = Set_New(compareBasicBlocks, NULL);

        struct Set *thisblockSuccessors = blockSuccessors[blockIndex];

        for (struct LinkedListNode *tacRunner = blocks[blockIndex]->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *thisTAC = tacRunner->data;
            switch (thisTAC->operation)
            {
            case tt_beq:
            case tt_bne:
            case tt_bgeu:
            case tt_bltu:
            case tt_bgtu:
            case tt_bleu:
            case tt_beqz:
            case tt_bnez:
            case tt_jmp:
                Set_Insert(thisblockSuccessors, blocks[thisTAC->operands[0].name.val]);
                break;

            default:
                break;
            }
        }
    }

    return blockSuccessors;
}

struct Set **generatePredecessors(struct BasicBlock **blocks, struct Set **successors, size_t nBlocks)
{
    struct Set **blockPredecessors = malloc(nBlocks * sizeof(struct LinkedList *));

    for (size_t blockIndex = 0; blockIndex < nBlocks; blockIndex++)
    {
        blockPredecessors[blockIndex] = Set_New(compareBasicBlocks, NULL);
    }

    for (size_t blockIndex = 0; blockIndex < nBlocks; blockIndex++)
    {
        for (struct LinkedListNode *successorRunner = successors[blockIndex]->elements->head; successorRunner != NULL; successorRunner = successorRunner->next)
        {
            struct BasicBlock *successor = successorRunner->data;
            Set_Insert(blockPredecessors[successor->labelNum], blocks[blockIndex]);
        }
    }

    return blockPredecessors;
}

struct IdfaContext *IdfaContext_Create(struct LinkedList *blocks)
{
    struct IdfaContext *wip = malloc(sizeof(struct IdfaContext));
    wip->nBlocks = blocks->size;
    wip->blocks = malloc(wip->nBlocks * sizeof(struct BasicBlock *));

    for (struct LinkedListNode *blockRunner = blocks->head; blockRunner != NULL; blockRunner = blockRunner->next)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        if (thisBlock->labelNum >= wip->nBlocks)
        {
            ErrorAndExit(ERROR_INTERNAL, "Block label number %zu exceeds number of blocks %zu in IdfaContext_Create", thisBlock->labelNum, blocks->size);
        }
        wip->blocks[thisBlock->labelNum] = thisBlock;
    }

    wip->successors = generateSuccessors(wip->blocks, wip->nBlocks);
    wip->predecessors = generatePredecessors(wip->blocks, wip->successors, wip->nBlocks);

    return wip;
}

void IdfaContext_Free(struct IdfaContext *context)
{
    free(context->blocks);
    for (size_t blockIndex = 0; blockIndex < context->nBlocks; blockIndex++)
    {
        Set_Free(context->successors[blockIndex]);
        Set_Free(context->predecessors[blockIndex]);
    }
    free(context->successors);
    free(context->predecessors);
    free(context);
}

struct Idfa *Idfa_Create(struct IdfaContext *context,
                         struct Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts),
                         void (*findGenKills)(struct Idfa *idfa),
                         int (*compareFacts)(void *factA, void *factB),
                         void (*printFact)(void *factData),
                         struct Set *(*fMeet)(struct Set *factsA, struct Set *factsB))
{
    struct Idfa *wip = malloc(sizeof(struct Idfa));
    wip->context = context;
    wip->compareFacts = compareFacts;
    wip->printFact = printFact;
    wip->fTransfer = fTransfer;
    wip->findGenKills = findGenKills;
    wip->fMeet = fMeet;

    wip->facts.in = malloc(wip->context->nBlocks * sizeof(struct Set *));
    wip->facts.out = malloc(wip->context->nBlocks * sizeof(struct Set *));
    wip->facts.gen = malloc(wip->context->nBlocks * sizeof(struct Set *));
    wip->facts.kill = malloc(wip->context->nBlocks * sizeof(struct Set *));

    for (size_t i = 0; i < wip->context->nBlocks; i++)
    {
        wip->facts.in[i] = Set_New(wip->compareFacts, NULL);
        wip->facts.out[i] = Set_New(wip->compareFacts, NULL);
        wip->facts.gen[i] = Set_New(wip->compareFacts, NULL);
        wip->facts.kill[i] = Set_New(wip->compareFacts, NULL);
    }

    return wip;
}

void Idfa_printFactsForBlock(struct Idfa *idfa, size_t blockIndex)
{
    printf("Block %zu facts:\n", blockIndex);

    printf("\tGen: ");
    for (struct LinkedListNode *factRunner = idfa->facts.gen[blockIndex]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        printf("[");
        idfa->printFact(factRunner->data);
        printf("] ");
    }

    printf("\n\tKill: ");
    for (struct LinkedListNode *factRunner = idfa->facts.kill[blockIndex]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        printf("[");
        idfa->printFact(factRunner->data);
        printf("] ");
    }

    printf("\n\tIn: ");
    for (struct LinkedListNode *factRunner = idfa->facts.in[blockIndex]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        printf("[");
        idfa->printFact(factRunner->data);
        printf("] ");
    }

    printf("\n\tOut: ");
    for (struct LinkedListNode *factRunner = idfa->facts.out[blockIndex]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        printf("[");
        idfa->printFact(factRunner->data);
        printf("] ");
    }
    printf("\n\n");
}

void Idfa_printFacts(struct Idfa *idfa)
{
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        Idfa_printFactsForBlock(idfa, blockIndex);
    }
}

void Idfa_AnalyzeForwards(struct Idfa *idfa)
{
    idfa->findGenKills(idfa);
    size_t iteration = 0;
    size_t nChangedOutputs = 0;
    do
    {
        // printf("idfa iteration %zu\n", iteration);
        nChangedOutputs = 0;

        // skip the entry block as we go using predecessors
        for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
        {
            // get rid of our previous "in" facts as we will generate them again
            // Idfa_printFactsForBlock(idfa, blockIndex);
            struct Set *oldInFacts = idfa->facts.in[blockIndex];

            // re-generate our "in" facts from the union of the "out" facts of all predecessors
            struct Set *newInFacts = NULL;
            for (struct LinkedListNode *predecessorRunner = idfa->context->predecessors[blockIndex]->elements->head; predecessorRunner != NULL; predecessorRunner = predecessorRunner->next)
            {
                struct BasicBlock *predecessor = predecessorRunner->data;
                struct Set *predOuts = idfa->facts.out[predecessor->labelNum];

                if (newInFacts == NULL)
                {
                    newInFacts = Set_Copy(predOuts);
                }
                else
                {
                    struct Set *metInFacts = idfa->fMeet(newInFacts, predOuts);
                    Set_Free(newInFacts);
                    newInFacts = metInFacts;
                }
            }
            if (newInFacts == NULL)
            {
                newInFacts = Set_New(oldInFacts->compareFunction, oldInFacts->dataFreeFunction);
            }
            Set_Free(oldInFacts);
            idfa->facts.in[blockIndex] = newInFacts;

            struct Set *transferred = idfa->fTransfer(idfa, idfa->context->blocks[blockIndex], newInFacts);
            if (transferred->elements->size != idfa->facts.out[blockIndex]->elements->size)
            {
                nChangedOutputs++;
            }

            Set_Free(idfa->facts.out[blockIndex]);
            idfa->facts.out[blockIndex] = transferred;
        }

        iteration++;
    } while (nChangedOutputs > 0);
}

void Idfa_Free(struct Idfa *idfa)
{
    for (size_t i = 0; i < idfa->context->nBlocks; i++)
    {
        Set_Free(idfa->facts.in[i]);
        Set_Free(idfa->facts.out[i]);
        Set_Free(idfa->facts.gen[i]);
        Set_Free(idfa->facts.kill[i]);
    }

    free(idfa->facts.in);
    free(idfa->facts.out);
    free(idfa->facts.gen);
    free(idfa->facts.kill);

    free(idfa);
}
