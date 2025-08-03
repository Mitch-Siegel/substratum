#include "idfa_reachingdefs.h"

#include "idfa_livevars.h"
#include "symtab_basicblock.h"
#include "util.h"

const size_t SIZE_T_PRINT_LENGTH = 20;
char *sprint_idfa_operand(void *data)
{
    struct TACOperand *operand = data;
    char *sprinted = tac_operand_sprint(operand);
    char *typename = type_get_name(tac_operand_get_non_cast_type(operand));
    char *castTypeName = type_get_name(&operand->castAsType);
    char *returned = malloc(strlen(sprinted) + strlen(typename) + strlen(castTypeName) + SIZE_T_PRINT_LENGTH);
    sprintf(returned, "%s(%s) %s %zu", typename, castTypeName, sprinted, operand->ssaNumber);
    free(typename);
    free(castTypeName);
    free(sprinted);
    return returned;
}

Set *reacing_defs_transfer(struct Idfa *idfa, struct BasicBlock *block, Set *facts)
{
    printf("ENTRY TO TRANSFER\n");
    Set *transferred = set_new(facts->freeData, facts->compareData);

    // transfer anything in GEN but not in KILL
    Iterator *factRunner = NULL;
    for (factRunner = set_begin(array_at(idfa->facts.gen, block->labelNum)); iterator_gettable(factRunner); iterator_next(factRunner))
    {
        struct TACOperand *examinedFact = iterator_get(factRunner);
        if (set_find(array_at(idfa->facts.kill, block->labelNum), examinedFact) == NULL)
        {
            set_try_insert(transferred, examinedFact);
        }
    }
    iterator_free(factRunner);
    factRunner = NULL;

    // transfer anything we get in not in KILL
    for (factRunner = set_begin(facts); iterator_gettable(factRunner); iterator_next(factRunner))
    {
        struct TACOperand *examinedFact = iterator_get(factRunner);
        // transfer anything not killed
        if (set_find(array_at(idfa->facts.kill, block->labelNum), examinedFact) == NULL)
        {
            set_try_insert(transferred, examinedFact);
        }
    }
    iterator_free(factRunner);
    factRunner = NULL;

    set_verify(transferred);

    return transferred;
}

void reacing_defs_find_gen_kills(struct Idfa *idfa)
{
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *genKillBlock = array_at(idfa->context->blocks, blockIndex);
        Set *highestSsas = set_new(NULL, tac_operand_compare_ignore_ssa_number);
        Iterator *tacRunner = NULL;
        Set *killedThisBlock = array_at(idfa->facts.kill, blockIndex);
        // killedThisBlock = set_new(NULL, killedThisBlock->compareData);
        for (tacRunner = list_begin(genKillBlock->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
        {
            struct TACLine *genKillLine = iterator_get(tacRunner);

            struct OperandUsages genKillLineUsages = get_operand_usages(genKillLine);

            while (genKillLineUsages.reads->size > 0)
            {
                struct TACOperand *readOperand = deque_pop_front(genKillLineUsages.reads);
                set_insert(killedThisBlock, readOperand);
            }

            while (genKillLineUsages.writes->size > 0)
            {
                struct TACOperand *writtenOperand = deque_pop_front(genKillLineUsages.writes);
                struct TACOperand *highestForThisOperand = set_find(highestSsas, writtenOperand);
                if (highestForThisOperand == NULL)
                {
                    set_insert(highestSsas, writtenOperand);
                }
                else
                {
                    size_t thisSsaNumber = writtenOperand->ssaNumber;
                    if (highestForThisOperand->ssaNumber < thisSsaNumber)
                    {
                        set_remove(highestSsas, writtenOperand);
                        set_insert(highestSsas, writtenOperand);
                    }
                }
            }
        }
        iterator_free(tacRunner);

        Iterator *highestSsaRunner = NULL;
        for (highestSsaRunner = set_begin(highestSsas); iterator_gettable(highestSsaRunner); iterator_next(highestSsaRunner))
        {
            set_insert(array_at(idfa->facts.gen, blockIndex), iterator_get(highestSsaRunner));
        }
        iterator_free(highestSsaRunner);

        set_free(highestSsas);
    }
}

struct Idfa *analyze_reaching_defs(struct IdfaContext *context)
{
    struct Idfa *reacingDefsIdfa = idfa_create(context,
                                               reacing_defs_transfer,
                                               reacing_defs_find_gen_kills,
                                               D_FORWARDS,
                                               tac_operand_compare,
                                               sprint_idfa_operand,
                                               set_union);

    return reacingDefsIdfa;
}