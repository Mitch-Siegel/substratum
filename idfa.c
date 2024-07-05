#include "idfa.h"
#include "log.h"
#include "symtab_basicblock.h"
#include "util.h"

struct Set **generate_successors(struct BasicBlock **blocks, size_t nBlocks)
{
    struct Set **blockSuccessors = malloc(nBlocks * sizeof(struct LinkedList *));

    for (size_t blockIndex = 0; blockIndex < nBlocks; blockIndex++)
    {
        // block pointers will be unique, so we can directly compare them
        blockSuccessors[blockIndex] = set_new(ssizet_compare, NULL);

        struct Set *thisblockSuccessors = blockSuccessors[blockIndex];

        for (struct LinkedListNode *tacRunner = blocks[blockIndex]->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *thisTac = tacRunner->data;
            switch (thisTac->operation)
            {
            case TT_BEQ:
            case TT_BNE:
            case TT_BGEU:
            case TT_BLTU:
            case TT_BGTU:
            case TT_BLEU:
            case TT_BEQZ:
            case TT_BNEZ:
            case TT_JMP:
                set_insert(thisblockSuccessors, blocks[thisTac->operands[0].name.val]);
                break;

            default:
                break;
            }
        }
    }

    return blockSuccessors;
}

struct Set **generate_predecessors(struct BasicBlock **blocks, struct Set **successors, size_t nBlocks)
{
    struct Set **blockPredecessors = malloc(nBlocks * sizeof(struct LinkedList *));

    for (size_t blockIndex = 0; blockIndex < nBlocks; blockIndex++)
    {
        // block pointers will always be unique, so we can directly compare them
        blockPredecessors[blockIndex] = set_new(ssizet_compare, NULL);
    }

    for (size_t blockIndex = 0; blockIndex < nBlocks; blockIndex++)
    {
        for (struct LinkedListNode *successorRunner = successors[blockIndex]->elements->head; successorRunner != NULL; successorRunner = successorRunner->next)
        {
            struct BasicBlock *successor = successorRunner->data;
            set_insert(blockPredecessors[successor->labelNum], blocks[blockIndex]);
        }
    }

    return blockPredecessors;
}

struct IdfaContext *idfa_context_create(struct LinkedList *blocks)
{
    struct IdfaContext *wip = malloc(sizeof(struct IdfaContext));
    wip->nBlocks = blocks->size;
    wip->blocks = malloc(wip->nBlocks * sizeof(struct BasicBlock *));

    for (struct LinkedListNode *blockRunner = blocks->head; blockRunner != NULL; blockRunner = blockRunner->next)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        if (thisBlock->labelNum >= wip->nBlocks)
        {
            InternalError("Block label number %zu exceeds number of blocks %zu in IdfaContext_Create", thisBlock->labelNum, blocks->size);
        }
        wip->blocks[thisBlock->labelNum] = thisBlock;
    }

    wip->successors = generate_successors(wip->blocks, wip->nBlocks);
    wip->predecessors = generate_predecessors(wip->blocks, wip->successors, wip->nBlocks);

    return wip;
}

void idfa_context_free(struct IdfaContext *context)
{
    free(context->blocks);
    for (size_t blockIndex = 0; blockIndex < context->nBlocks; blockIndex++)
    {
        set_free(context->successors[blockIndex]);
        set_free(context->predecessors[blockIndex]);
    }
    free(context->successors);
    free(context->predecessors);
    free(context);
}

struct Idfa *idfa_create(struct IdfaContext *context,
                         struct Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts),
                         void (*findGenKills)(struct Idfa *idfa),
                         enum IDFA_ANALYSIS_DIRECTION direction,
                         ssize_t (*compareFacts)(void *factA, void *factB),
                         void (*printFact)(void *factData),
                         struct Set *(*fMeet)(struct Set *factsA, struct Set *factsB))
{
    struct Idfa *wip = malloc(sizeof(struct Idfa));
    wip->context = context;
    wip->compare_facts = compareFacts;
    wip->print_fact = printFact;
    wip->f_transfer = fTransfer;
    wip->find_gen_kills = findGenKills;
    wip->f_meet = fMeet;
    wip->direction = direction;

    wip->facts.in = malloc(wip->context->nBlocks * sizeof(struct Set *));
    wip->facts.out = malloc(wip->context->nBlocks * sizeof(struct Set *));
    wip->facts.gen = malloc(wip->context->nBlocks * sizeof(struct Set *));
    wip->facts.kill = malloc(wip->context->nBlocks * sizeof(struct Set *));

    for (size_t i = 0; i < wip->context->nBlocks; i++)
    {
        wip->facts.in[i] = set_new(wip->compare_facts, NULL);
        wip->facts.out[i] = set_new(wip->compare_facts, NULL);
        wip->facts.gen[i] = set_new(wip->compare_facts, NULL);
        wip->facts.kill[i] = set_new(wip->compare_facts, NULL);
    }

    idfa_analyze(wip);

    return wip;
}

void idfa_print_facts_for_block(struct Idfa *idfa, size_t blockIndex)
{
    printf("Block %zu facts:\n", blockIndex);

    printf("\tGen: ");
    for (struct LinkedListNode *factRunner = idfa->facts.gen[blockIndex]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        printf("[");
        idfa->print_fact(factRunner->data);
        printf("] ");
    }

    printf("\n\tKill: ");
    for (struct LinkedListNode *factRunner = idfa->facts.kill[blockIndex]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        printf("[");
        idfa->print_fact(factRunner->data);
        printf("] ");
    }

    printf("\n\tIn: ");
    for (struct LinkedListNode *factRunner = idfa->facts.in[blockIndex]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        printf("[");
        idfa->print_fact(factRunner->data);
        printf("] ");
    }

    printf("\n\tOut: ");
    for (struct LinkedListNode *factRunner = idfa->facts.out[blockIndex]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        printf("[");
        idfa->print_fact(factRunner->data);
        printf("] ");
    }
    printf("\n\n");
}

void idfa_print_facts(struct Idfa *idfa)
{
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        idfa_print_facts_for_block(idfa, blockIndex);
    }
}

void idfa_analyze_forwards(struct Idfa *idfa)
{
    idfa->find_gen_kills(idfa);
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
                    newInFacts = set_copy(predOuts);
                }
                else
                {
                    struct Set *metInFacts = idfa->f_meet(newInFacts, predOuts);
                    set_free(newInFacts);
                    newInFacts = metInFacts;
                }
            }
            if (newInFacts == NULL)
            {
                newInFacts = set_new(oldInFacts->compareFunction, oldInFacts->dataFreeFunction);
            }
            set_free(oldInFacts);
            idfa->facts.in[blockIndex] = newInFacts;

            struct Set *transferred = idfa->f_transfer(idfa, idfa->context->blocks[blockIndex], newInFacts);
            if (transferred->elements->size != idfa->facts.out[blockIndex]->elements->size)
            {
                nChangedOutputs++;
            }

            set_free(idfa->facts.out[blockIndex]);
            idfa->facts.out[blockIndex] = transferred;
        }

        iteration++;
    } while (nChangedOutputs > 0);
}

void idfa_analyze_backwards(struct Idfa *idfa)
{
    InternalError("Backwards dataflow analysis not implemented");
}

void idfa_analyze(struct Idfa *idfa)
{
    switch (idfa->direction)
    {
    case D_FORWARDS:
        idfa_analyze_forwards(idfa);
        break;

    case D_BACKWARDS:
        idfa_analyze_backwards(idfa);
        break;
    }
}

void idfa_redo(struct Idfa *idfa)
{
    for (size_t i = 0; i < idfa->context->nBlocks; i++)
    {
        set_free(idfa->facts.in[i]);
        idfa->facts.in[i] = set_new(idfa->compare_facts, NULL);

        set_free(idfa->facts.out[i]);
        idfa->facts.out[i] = set_new(idfa->compare_facts, NULL);

        set_free(idfa->facts.gen[i]);
        idfa->facts.gen[i] = set_new(idfa->compare_facts, NULL);

        set_free(idfa->facts.kill[i]);
        idfa->facts.kill[i] = set_new(idfa->compare_facts, NULL);
    }
    idfa_analyze(idfa);
}

void idfa_free(struct Idfa *idfa)
{
    for (size_t i = 0; i < idfa->context->nBlocks; i++)
    {
        set_free(idfa->facts.in[i]);
        set_free(idfa->facts.out[i]);
        set_free(idfa->facts.gen[i]);
        set_free(idfa->facts.kill[i]);
    }

    free(idfa->facts.in);
    free(idfa->facts.out);
    free(idfa->facts.gen);
    free(idfa->facts.kill);

    free(idfa);
}
