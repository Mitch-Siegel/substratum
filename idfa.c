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
        blockSuccessors[blockIndex] = Set_New(compareBasicBlocks);

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
        blockPredecessors[blockIndex] = Set_New(compareBasicBlocks);

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

struct Idfa *Idfa_Create(struct LinkedList *blocks,
                         struct Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts),
                         void (*findGenKills)(struct Idfa *idfa),
                         int (*compareFacts)(void *factA, void *factB))
{
    struct Idfa *wip = malloc(sizeof(struct Idfa));
    wip->context = IdfaContext_Create(blocks);
    wip->fTransfer = fTransfer;
    wip->findGenKills = findGenKills;
    wip->compareFacts = compareFacts;

    for(size_t i = 0; i < wip->context->nBlocks; i++)
    {
        wip->facts.in[i] = Set_New(wip->compareFacts);
        wip->facts.out[i] = Set_New(wip->compareFacts);
        wip->facts.gen[i] = Set_New(wip->compareFacts);
        wip->facts.kill[i] = Set_New(wip->compareFacts);
    }

    return wip;
}

void Idfa_AnalyzeForwards(struct Idfa *idfa)
{
    size_t iteration = 0;
    size_t nChangedOutputs = 0;
    do
    {
        nChangedOutputs = 0;

        // skip the entry block as we go using predecessors
        for (size_t i = 1; i < idfa->context->nBlocks; i++)
        {
            Set_Free(idfa->facts.in[i]);

            struct Set *newInFacts = Set_New(idfa->compareFacts);
            idfa->facts.in[i] = newInFacts;

            for(struct LinkedListNode *predecessorRunner = idfa->context->predecessors[i]->elements->head; predecessorRunner != NULL; predecessorRunner = predecessorRunner->next)
            {
                struct BasicBlock *predecessor = predecessorRunner->data;
                Set_Merge(newInFacts, idfa->facts.out[predecessor->labelNum]);
            }

            struct Set *transferred = idfa->fTransfer(idfa, idfa->context->blocks[i], newInFacts);
            if(transferred->elements->size != idfa->facts.out[i]->elements->size)
            {
                nChangedOutputs++;
            }

            Set_Free(idfa->facts.out[i]);
            idfa->facts.out[i] = transferred;
        }

        iteration++;
    } while (nChangedOutputs > 0);
}