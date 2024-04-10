#include "livevars.h"

#include "symtab_basicblock.h"
#include "util.h"

int compareTacOperand(void *dataA, void *dataB)
{
    struct TACOperand *operandA = dataA;
    struct TACOperand *operandB = dataB;

    if (Type_Compare(&operandA->type, &operandB->type))
    {
        return 1;
    }

    if (Type_Compare(&operandA->castAsType, &operandB->castAsType))
    {
        return 1;
    }

    if ((operandA->permutation != vp_literal && (operandB->permutation != vp_literal)))
    {
        if (strcmp(operandA->name.str, operandB->name.str))
        {
            return 1;
        }
    }
    else if (operandA->permutation != operandB->permutation)
    {
        return 1;
    }

    if (operandA->ssaNumber != operandB->ssaNumber)
    {
        return 1;
    }

    return 0;
}

struct Set *liveVars_transfer(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts)
{
    struct Set *transferred = Set_New(idfa->compareFacts);

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

void liveVars_findGenKills(struct Idfa *idfa)
{
}

struct Idfa *analyzeLiveVars(struct IdfaContext *context)
{
    struct Idfa *liveVarsIdfa = Idfa_Create(context,
                                            liveVars_transfer,
                                            liveVars_findGenKills,
                                            compareTacOperand);

    Idfa_AnalyzeForwards(liveVarsIdfa);
    return liveVarsIdfa;
}