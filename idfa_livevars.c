#include "idfa_livevars.h"

#include "log.h"
#include "symtab_basicblock.h"
#include "util.h"

Set *live_vars_transfer(struct Idfa *idfa, struct BasicBlock *block, Set *facts)
{
    Set *transferred = set_copy(array_at(idfa->facts.gen, block->labelNum));

    Iterator *factRunner = NULL;
    for (factRunner = set_begin(facts); iterator_gettable(factRunner); iterator_next(factRunner))
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

void live_vars_find_gen_kills(struct Idfa *idfa)
{
    for (size_t blockIndex = 0; blockIndex < idfa->context->blocks->size; blockIndex++)
    {
        struct BasicBlock *genKillBlock = array_at(idfa->context->blocks, blockIndex);
        Iterator *tacRunner = NULL;
        for (tacRunner = list_begin(genKillBlock->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
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
                    set_insert(array_at(idfa->facts.gen, blockIndex), &genKillLine->operands[operandIndex]);

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
                                            set_union);

    return liveVarsIdfa;
}