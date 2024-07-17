#include "idfa_reachingdefs.h"

#include "idfa_livevars.h"
#include "symtab_basicblock.h"
#include "util.h"

Set *reacing_defs_transfer(struct Idfa *idfa, struct BasicBlock *block, Set *facts)
{
    Set *transferred = set_new(facts->freeData, facts->compareData);

    // transfer anything in GEN but not in KILL
    Iterator *factRunner = NULL;
    for (factRunner = set_begin(array_at(idfa->facts.gen, block->labelNum)); iterator_valid(factRunner); iterator_next(factRunner))
    {
        struct TACOperand *examinedFact = iterator_get(factRunner);
        if (set_find(array_at(idfa->facts.kill, block->labelNum), examinedFact) == NULL)
        {
            set_insert(transferred, examinedFact);
        }
    }
    iterator_free(factRunner);
    factRunner = NULL;

    // transfer anything we get in not in KILL
    for (factRunner = set_begin(facts); iterator_valid(factRunner); iterator_next(factRunner))
    {
        struct TACOperand *examinedFact = iterator_get(factRunner);
        // transfer anything not killed
        if (set_find(array_at(idfa->facts.kill, block->labelNum), examinedFact) == NULL)
        {
            set_insert(transferred, examinedFact);
        }
    }

    return transferred;
}

void reacing_defs_find_gen_kills(struct Idfa *idfa)
{
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *genKillBlock = array_at(idfa->context->blocks, blockIndex);
        Set *highestSsas = set_new(NULL, tac_operand_compare_ignore_ssa_number);
        Iterator *tacRunner = NULL;
        for (tacRunner = list_begin(genKillBlock->TACList); iterator_valid(tacRunner); iterator_next(tacRunner))
        {
            struct TACLine *genKillLine = iterator_get(tacRunner);
            for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
            {
                switch (get_use_of_operand(genKillLine, operandIndex))
                {
                case U_UNUSED:
                    break;

                case U_READ:
                    set_insert(array_at(idfa->facts.kill, blockIndex), &genKillLine->operands[operandIndex]);
                    break;

                case U_WRITE:
                {
                    struct TACOperand *highestForThisOperand = set_find(highestSsas, &genKillLine->operands[operandIndex]);
                    if (highestForThisOperand == NULL)
                    {
                        set_insert(highestSsas, &genKillLine->operands[operandIndex]);
                    }
                    else
                    {
                        size_t thisSsaNumber = genKillLine->operands[operandIndex].ssaNumber;
                        if (highestForThisOperand->ssaNumber < thisSsaNumber)
                        {
                            set_remove(highestSsas, &genKillLine->operands[operandIndex]);
                            set_insert(highestSsas, &genKillLine->operands[operandIndex]);
                        }
                    }
                }
                break;
                }
            }
        }
        iterator_free(tacRunner);

        Iterator *highestSsaRunner = NULL;
        for (highestSsaRunner = set_begin(highestSsas); iterator_valid(highestSsaRunner); iterator_next(highestSsaRunner))
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
                                               tac_operand_sprint,
                                               set_union);

    return reacingDefsIdfa;
}