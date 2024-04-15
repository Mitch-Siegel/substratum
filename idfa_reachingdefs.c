#include "idfa_reachingdefs.h"

#include "idfa_livevars.h"
#include "symtab_basicblock.h"
#include "util.h"

struct Set *reacingDefs_transfer(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts)
{
    struct Set *transferred = Set_New(facts->compareFunction, facts->dataFreeFunction);

    // transfer anything in GEN but not in KILL
    for (struct LinkedListNode *factRunner = idfa->facts.gen[block->labelNum]->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        struct TACOperand *examinedFact = factRunner->data;
        if (Set_Find(idfa->facts.kill[block->labelNum], examinedFact) == NULL)
        {
            Set_Insert(transferred, examinedFact);
        }
    }

    // transfer anything we get in not in KILL
    for (struct LinkedListNode *factRunner = facts->elements->head; factRunner != NULL; factRunner = factRunner->next)
    {
        struct TACOperand *examinedFact = factRunner->data;
        // transfer anything not killed
        if (Set_Find(idfa->facts.kill[block->labelNum], examinedFact) == NULL)
        {
            Set_Insert(transferred, examinedFact);
        }
    }

    return transferred;
}

void reacingDefs_findGenKills(struct Idfa *idfa)
{
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *genKillBlock = idfa->context->blocks[blockIndex];
        struct Set *highestSsas = Set_New(TACOperand_CompareIgnoreSsaNumber, NULL);
        for (struct LinkedListNode *tacRunner = genKillBlock->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
        {
            struct TACLine *genKillLine = tacRunner->data;
            for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
            {
                switch (getUseOfOperand(genKillLine, operandIndex))
                {
                case u_unused:
                    break;

                case u_read:
                    Set_Insert(idfa->facts.kill[blockIndex], &genKillLine->operands[operandIndex]);
                    break;

                case u_write:
                {
                    struct TACOperand *highestForThisOperand = Set_Find(highestSsas, &genKillLine->operands[operandIndex]);
                    if (highestForThisOperand == NULL)
                    {
                        Set_Insert(highestSsas, &genKillLine->operands[operandIndex]);
                    }
                    else
                    {
                        size_t thisSsaNumber = genKillLine->operands[operandIndex].ssaNumber;
                        if (highestForThisOperand->ssaNumber < thisSsaNumber)
                        {
                            Set_Delete(highestSsas, &genKillLine->operands[operandIndex]);
                            Set_Insert(highestSsas, &genKillLine->operands[operandIndex]);
                        }
                    }
                }
                break;
                }
            }
        }

        for (struct LinkedListNode *highestSsaRunner = highestSsas->elements->head; highestSsaRunner != NULL; highestSsaRunner = highestSsaRunner->next)
        {
            Set_Insert(idfa->facts.gen[blockIndex], highestSsaRunner->data);
        }

        Set_Free(highestSsas);
    }
}

struct Idfa *analyzeReachingDefs(struct IdfaContext *context)
{
    struct Idfa *reacingDefsIdfa = Idfa_Create(context,
                                               reacingDefs_transfer,
                                               reacingDefs_findGenKills,
                                               d_forwards,
                                               TACOperand_Compare,
                                               printTACOperand,
                                               Set_Union);

    return reacingDefsIdfa;
}