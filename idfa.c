#include "idfa.h"
#include "log.h"
#include "symtab_basicblock.h"
#include "util.h"

#include "mbcl/set.h"

// returns an array of sets - index i in the array is a set containing the blocks which are successors of block i
Array *generate_successors(Array *blocks)
{
    Array *blockSuccessors = array_new((MBCL_DATA_FREE_FUNCTION)set_free, blocks->size);
    for (size_t blockIndex = 0; blockIndex < blocks->size; blockIndex++)
    {
        // block pointers will be unique, so we can directly compare them
        Set *thisblockSuccessors = set_new(NULL, pointer_compare);
        array_emplace(blockSuccessors, blockIndex, thisblockSuccessors);

        struct BasicBlock *thisBlock = array_at(blocks, blockIndex);

        Iterator *successorRunner = NULL;
        for (successorRunner = set_begin(thisBlock->successors); iterator_gettable(successorRunner); iterator_next(successorRunner))
        {
            ssize_t *successorLabel = iterator_get(successorRunner);
            set_insert(thisblockSuccessors, array_at(blocks, *successorLabel));
        }
        iterator_free(successorRunner);
    }

    return blockSuccessors;
}

Array *generate_predecessors(Array *blocks, Array *successors)
{
    Array *blockPredecessors = array_new((MBCL_DATA_FREE_FUNCTION)set_free, blocks->size);

    for (size_t blockIndex = 0; blockIndex < blocks->size; blockIndex++)
    {
        // block pointers will always be unique, so we can directly compare them
        array_emplace(blockPredecessors, blockIndex, set_new(NULL, pointer_compare));
    }

    for (size_t blockIndex = 0; blockIndex < blocks->size; blockIndex++)
    {
        Iterator *successorRunner = NULL;
        for (successorRunner = set_begin(array_at(successors, blockIndex)); iterator_gettable(successorRunner); iterator_next(successorRunner))
        {
            struct BasicBlock *successor = iterator_get(successorRunner);
            set_insert(array_at(blockPredecessors, successor->labelNum), array_at(blocks, blockIndex));
        }
        iterator_free(successorRunner);
    }

    return blockPredecessors;
}

struct IdfaContext *idfa_context_create(char *name, List *blocks)
{
    struct IdfaContext *wip = malloc(sizeof(struct IdfaContext));
    wip->name = name;
    size_t nBlocks = blocks->size;
    wip->nBlocks = nBlocks;
    wip->blocks = array_new(NULL, nBlocks);

    Iterator *blockRunner = NULL;
    for (blockRunner = list_begin(blocks); iterator_gettable(blockRunner); iterator_next(blockRunner))
    {
        struct BasicBlock *thisBlock = iterator_get(blockRunner);
        if (thisBlock->labelNum >= nBlocks)
        {
            InternalError("Block label number %zu exceeds number of blocks %zu in IdfaContext_Create", thisBlock->labelNum, blocks->size);
        }
        array_emplace(wip->blocks, thisBlock->labelNum, thisBlock);
    }
    iterator_free(blockRunner);

    wip->successors = generate_successors(wip->blocks);
    wip->predecessors = generate_predecessors(wip->blocks, wip->successors);

    return wip;
}

void idfa_context_free(struct IdfaContext *context)
{
    array_free(context->blocks);
    array_free(context->predecessors);
    array_free(context->successors);
    free(context);
}

struct Idfa *idfa_create(struct IdfaContext *context,
                         Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, Set *facts),
                         void (*findGenKills)(struct Idfa *idfa),
                         enum IDFA_ANALYSIS_DIRECTION direction,
                         ssize_t (*compareFacts)(void *factA, void *factB),
                         char *(*sprintFact)(void *factData),
                         Set *(*fMeet)(Set *factsA, Set *factsB))
{
    struct Idfa *wip = malloc(sizeof(struct Idfa));
    wip->context = context;
    wip->compareFacts = compareFacts;
    wip->sprintFact = sprintFact;
    wip->fTransfer = fTransfer;
    wip->findGenKills = findGenKills;
    wip->fMeet = fMeet;
    wip->direction = direction;

    // fixme: pointer for set_free
    wip->facts.in = array_new((void (*)(void *))rb_tree_free, wip->context->blocks->size);
    wip->facts.out = array_new((void (*)(void *))rb_tree_free, wip->context->blocks->size);
    wip->facts.gen = array_new((void (*)(void *))rb_tree_free, wip->context->blocks->size);
    wip->facts.kill = array_new((void (*)(void *))rb_tree_free, wip->context->blocks->size);

    for (size_t i = 0; i < wip->context->nBlocks; i++)
    {
        array_emplace(wip->facts.in, i, set_new(NULL, wip->compareFacts));
        array_emplace(wip->facts.out, i, set_new(NULL, wip->compareFacts));
        array_emplace(wip->facts.gen, i, set_new(NULL, wip->compareFacts));
        array_emplace(wip->facts.kill, i, set_new(NULL, wip->compareFacts));
    }

    idfa_analyze(wip);

    return wip;
}

void idfa_print_facts_for_block(struct Idfa *idfa, size_t blockIndex)
{
    printf("Block %zu facts:\n", blockIndex);

    printf("\tGen: ");
    Iterator *factRunner = NULL;
    for (factRunner = set_begin(array_at(idfa->facts.gen, blockIndex)); iterator_gettable(factRunner); iterator_next(factRunner))
    {
        char *sprintedFact = idfa->sprintFact(iterator_get(factRunner));
        printf("[%s] ", sprintedFact);
        free(sprintedFact);
    }
    iterator_free(factRunner);
    factRunner = NULL;

    printf("\n\tKill: ");
    for (factRunner = set_begin(array_at(idfa->facts.kill, blockIndex)); iterator_gettable(factRunner); iterator_next(factRunner))
    {
        char *sprintedFact = idfa->sprintFact(iterator_get(factRunner));
        printf("[%s] ", sprintedFact);
        free(sprintedFact);
    }
    iterator_free(factRunner);
    factRunner = NULL;

    printf("\n\tIn: ");
    for (factRunner = set_begin(array_at(idfa->facts.in, blockIndex)); iterator_gettable(factRunner); iterator_next(factRunner))
    {
        char *sprintedFact = idfa->sprintFact(iterator_get(factRunner));
        printf("[%s] ", sprintedFact);
        free(sprintedFact);
    }
    iterator_free(factRunner);
    factRunner = NULL;

    printf("\n\tOut: ");
    for (factRunner = set_begin(array_at(idfa->facts.out, blockIndex)); iterator_gettable(factRunner); iterator_next(factRunner))
    {
        char *sprintedFact = idfa->sprintFact(iterator_get(factRunner));
        printf("[%s] ", sprintedFact);
        free(sprintedFact);
    }
    printf("\n\n");
    iterator_free(factRunner);
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
            printf("analyze block %zu\n", blockIndex);
            // get rid of our previous "in" facts as we will generate them again
            // Idfa_printFactsForBlock(idfa, blockIndex);
            Set *oldInFacts = array_at(idfa->facts.in, blockIndex);

            // re-generate our "in" facts from the union of the "out" facts of all predecessors
            Set *newInFacts = NULL;
            Iterator *predecessorRunner = NULL;
            for (predecessorRunner = set_begin(array_at(idfa->context->predecessors, blockIndex)); iterator_gettable(predecessorRunner); iterator_next(predecessorRunner))
            {
                struct BasicBlock *predecessor = iterator_get(predecessorRunner);
                Set *predOuts = array_at(idfa->facts.out, predecessor->labelNum);
                set_verify(predOuts);

                if (newInFacts == NULL)
                {
                    idfa_print_facts_for_block(idfa, blockIndex);
                    // idfa_print_facts(idfa);
                    printf("\n");
                    printf("copy predouts from %zu\n", predecessor->labelNum);
                    fflush(stdout);
                    newInFacts = set_copy(predOuts);
                }
                else
                {
                    Set *metInFacts = idfa->fMeet(newInFacts, predOuts);
                    set_verify(metInFacts);
                    set_free(newInFacts);
                    newInFacts = metInFacts;
                }
            }
            iterator_free(predecessorRunner);

            if (newInFacts == NULL)
            {
                newInFacts = set_new(oldInFacts->freeData, oldInFacts->compareData);
            }
            set_free(oldInFacts);
            array_emplace(idfa->facts.in, blockIndex, newInFacts);

            Set *transferred = idfa->fTransfer(idfa, array_at(idfa->context->blocks, blockIndex), newInFacts);
            set_verify(transferred);
            if (transferred->size != ((Set *)array_at(idfa->facts.out, blockIndex))->size)
            {
                nChangedOutputs++;
            }
            set_free(array_at(idfa->facts.out, blockIndex));
            array_emplace(idfa->facts.out, blockIndex, transferred);
        }

        printf("END OF ITERATION %zu:\n", iteration);
        idfa_print_facts(idfa);
        printf("\n");

        iteration++;
    } while (nChangedOutputs > 0);

    log(LOG_WARNING, "idfa reached fixpoint");
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
        set_free(array_at(idfa->facts.in, i));
        array_emplace(idfa->facts.in, i, set_new(NULL, idfa->compareFacts));

        set_free(array_at(idfa->facts.out, i));
        array_emplace(idfa->facts.out, i, set_new(NULL, idfa->compareFacts));

        set_free(array_at(idfa->facts.gen, i));
        array_emplace(idfa->facts.gen, i, set_new(NULL, idfa->compareFacts));

        set_free(array_at(idfa->facts.kill, i));
        array_emplace(idfa->facts.kill, i, set_new(NULL, idfa->compareFacts));
    }
    idfa_analyze(idfa);
}

void idfa_free(struct Idfa *idfa)
{
    array_free(idfa->facts.in);
    array_free(idfa->facts.out);
    array_free(idfa->facts.gen);
    array_free(idfa->facts.kill);

    free(idfa);
}
