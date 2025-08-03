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
            struct OperandUsages genKillLineUsages = get_operand_usages(genKillLine);

            while (genKillLineUsages.reads->size > 0)
            {
                struct TACOperand *readOperand = deque_pop_front(genKillLineUsages.reads);
                set_insert(array_at(idfa->facts.kill, blockIndex), readOperand);
            }

            while (genKillLineUsages.writes->size > 0)
            {
                struct TACOperand *writeOperand = deque_pop_front(genKillLineUsages.writes);
                set_insert(array_at(idfa->facts.gen, blockIndex), writeOperand);
            }

            deque_free(genKillLineUsages.reads);
            deque_free(genKillLineUsages.writes);
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