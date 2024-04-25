#include "tac_operand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Type *TACOperand_GetType(struct TACOperand *operand)
{
    if (operand->castAsType.basicType != vt_null)
    {
        return &operand->castAsType;
    }

    return &operand->type;
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

ssize_t TACOperand_CompareIgnoreSsaNumber(void *dataA, void *dataB)
{
    struct TACOperand *operandA = dataA;
    struct TACOperand *operandB = dataB;

    ssize_t result = Type_Compare(&operandA->type, &operandB->type);

    if (result)
    {
        return result;
    }

    result = Type_Compare(&operandA->castAsType, &operandB->castAsType);

    if (result)
    {
        return result;
    }

    if ((operandA->permutation != vp_literal && (operandB->permutation != vp_literal)))
    {
        result = strcmp(operandA->name.str, operandB->name.str);
        if (result)
        {
            return result;
        }
    }
    else if (operandA->permutation != operandB->permutation)
    {
        return 1;
    }

    return 0;
}

ssize_t TACOperand_Compare(void *dataA, void *dataB)
{
    struct TACOperand *operandA = dataA;
    struct TACOperand *operandB = dataB;

    ssize_t result = TACOperand_CompareIgnoreSsaNumber(dataA, dataB);

    if (result != 0)
    {
        return result;
    }

    return ((ssize_t)operandA->ssaNumber - (ssize_t)operandB->ssaNumber);
}