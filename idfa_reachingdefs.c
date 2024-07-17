#include "idfa_reachingdefs.h"

#include "idfa_livevars.h"
#include "symtab_basicblock.h"
#include "util.h"

struct Set *reacing_defs_transfer(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts)
{
    struct Set *transferred = old_set_new(facts->compareFunction, facts->dataFreeFunction);

    // transfer anything in GEN but not in KILL
    for (struct LinkedListNode *factRunner = idfa->facts.gen[block->labelNum]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        struct TACOperand *examinedFact = factRunner->data;
        if (old_set_find(idfa->facts.kill[block->labelNum], examinedFact) == NULL)
        {
            old_set_insert(transferred, examinedFact);
        }
    }

    // transfer anything we get in not in KILL
    for (struct LinkedListNode *factRunner = facts->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        struct TACOperand *examinedFact = factRunner->data;
        // transfer anything not killed
        if (old_set_find(idfa->facts.kill[block->labelNum], examinedFact) == NULL)
        {
            old_set_insert(transferred, examinedFact);
        }
    }

    return transferred;
}

void reacing_defs_find_gen_kills(struct Idfa *idfa)
{
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *genKillBlock = idfa->context->blocks[blockIndex];
        struct Set *highestSsas = old_set_new(tac_operand_compare_ignore_ssa_number, NULL);
        for (struct LinkedListNode *tacRunner = genKillBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *genKillLine = tacRunner->data;
            for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
            {
                switch (get_use_of_operand(genKillLine, operandIndex))
                {
                case U_UNUSED:
                    break;

                case U_READ:
                    old_set_insert(idfa->facts.kill[blockIndex], &genKillLine->operands[operandIndex]);
                    break;

                case U_WRITE:
                {
                    struct TACOperand *highestForThisOperand = old_set_find(highestSsas, &genKillLine->operands[operandIndex]);
                    if (highestForThisOperand == NULL)
                    {
                        old_set_insert(highestSsas, &genKillLine->operands[operandIndex]);
                    }
                    else
                    {
                        size_t thisSsaNumber = genKillLine->operands[operandIndex].ssaNumber;
                        if (highestForThisOperand->ssaNumber < thisSsaNumber)
                        {
                            old_set_delete(highestSsas, &genKillLine->operands[operandIndex]);
                            old_set_insert(highestSsas, &genKillLine->operands[operandIndex]);
                        }
                    }
                }
                break;
                }
            }
        }

        for (struct LinkedListNode *highestSsaRunner = highestSsas->elements->head; highestSsaRunner != NULL; highestSsaRunner = highestSsaRunner->next)
        {
            old_set_insert(idfa->facts.gen[blockIndex], highestSsaRunner->data);
        }

        old_set_free(highestSsas);
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
                                               old_set_union);

    return reacingDefsIdfa;
}