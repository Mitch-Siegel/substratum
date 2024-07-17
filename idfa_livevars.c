#include "idfa_livevars.h"

#include "log.h"
#include "symtab_basicblock.h"
#include "util.h"

struct Set *live_vars_transfer(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts)
{
    struct Set *transferred = old_set_copy(idfa->facts.gen[block->labelNum]);

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

void live_vars_find_gen_kills(struct Idfa *idfa)
{
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *genKillBlock = idfa->context->blocks[blockIndex];
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
                    if (genKillLine->operands[operandIndex].name.str == NULL)
                    {
                        InternalError("NULL OPERAND");
                    }
                    break;

                case U_WRITE:
                    if (genKillLine->operands[operandIndex].name.str == NULL)
                    {
                        InternalError("NULL OPERAND");
                    }
                    old_set_insert(idfa->facts.gen[blockIndex], &genKillLine->operands[operandIndex]);

                    break;
                }
            }
        }
    }
}

struct Idfa *analyze_live_vars(struct IdfaContext *context)
{
    struct Idfa *liveVarsIdfa = idfa_create(context,
                                            live_vars_transfer,
                                            live_vars_find_gen_kills,
                                            D_FORWARDS,
                                            tac_operand_compare,
                                            tac_operand_sprint,
                                            old_set_union);

    return liveVarsIdfa;
}