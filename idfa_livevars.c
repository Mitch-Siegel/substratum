#include "idfa_livevars.h"

#include "symtab_basicblock.h"
#include "util.h"
#include "log.h"

struct Set *liveVars_transfer(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts)
{
    struct Set *transferred = Set_Copy(idfa->facts.gen[block->labelNum]);

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
    for (size_t blockIndex = 0; blockIndex < idfa->context->nBlocks; blockIndex++)
    {
        struct BasicBlock *genKillBlock = idfa->context->blocks[blockIndex];
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
                    if (genKillLine->operands[operandIndex].name.str == NULL)
                    {
                        InternalError("NULL OPERAND");
                    }
                    printf("READ: ");
                    printTACOperand(&genKillLine->operands[operandIndex]);
                    printf("\n");
                    break;

                case u_write:
                    if (genKillLine->operands[operandIndex].name.str == NULL)
                    {
                        InternalError("NULL OPERAND");
                    }
                    Set_Insert(idfa->facts.gen[blockIndex], &genKillLine->operands[operandIndex]);
                    printf("WRITE: ");
                    printTACOperand(&genKillLine->operands[operandIndex]);
                    printf("\n");

                    break;
                }
            }
        }
    }
}

struct Idfa *analyzeLiveVars(struct IdfaContext *context)
{
    struct Idfa *liveVarsIdfa = Idfa_Create(context,
                                            liveVars_transfer,
                                            liveVars_findGenKills,
                                            d_forwards,
                                            TACOperand_Compare,
                                            printTACOperand,
                                            Set_Union);

    return liveVarsIdfa;
}