#include "idfa_livevars.h"

#include "symtab_basicblock.h"
#include "util.h"

int compareTacOperandIgnoreSsaNumber(void *dataA, void *dataB)
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

    return 0;
}

int compareTacOperand(void *dataA, void *dataB)
{
    struct TACOperand *operandA = dataA;
    struct TACOperand *operandB = dataB;

    if (compareTacOperandIgnoreSsaNumber(dataA, dataB))
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
                        ErrorAndExit(ERROR_CODE, "NULL OPERAND\n");
                    }
                    printf("READ: ");
                    printTACOperand(&genKillLine->operands[operandIndex]);
                    printf("\n");
                    break;

                case u_write:
                    if (genKillLine->operands[operandIndex].name.str == NULL)
                    {
                        ErrorAndExit(ERROR_CODE, "NULL OPERAND\n");
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

void printTACOperand(void *operandData)
{
    struct TACOperand *operand = operandData;
    char *typeName = Type_GetName(&operand->type);
    printf("%s", typeName);
    if (operand->castAsType.basicType != vt_null)

    {
        char *castAsTypeName = Type_GetName(&operand->castAsType);
        printf("(%s)", castAsTypeName);
        free(castAsTypeName);
    }
    printf(" %s_%zu", operand->name.str, operand->ssaNumber);
    free(typeName);
}

struct Idfa *analyzeLiveVars(struct IdfaContext *context)
{
    struct Idfa *liveVarsIdfa = Idfa_Create(context,
                                            liveVars_transfer,
                                            liveVars_findGenKills,
                                            d_forwards,
                                            compareTacOperand,
                                            printTACOperand,
                                            Set_Union);

    return liveVarsIdfa;
}