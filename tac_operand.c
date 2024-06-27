#include "tac_operand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Type *tac_operand_get_type(struct TACOperand *operand)
{
    if (operand->castAsType.basicType != VT_NULL)
    {
        return &operand->castAsType;
    }

    return &operand->type;
}

void print_tac_operand(void *operandData)
{
    struct TACOperand *operand = operandData;
    char *typeName = type_get_name(&operand->type);
    printf("%s", typeName);
    if (operand->castAsType.basicType != VT_NULL)

    {
        char *castAsTypeName = type_get_name(&operand->castAsType);
        printf("(%s)", castAsTypeName);
        free(castAsTypeName);
    }
    printf(" %s_%zu", operand->name.str, operand->ssaNumber);
    free(typeName);
}

ssize_t tac_operand_compare_ignore_ssa_number(void *dataA, void *dataB)
{
    struct TACOperand *operandA = dataA;
    struct TACOperand *operandB = dataB;

    ssize_t result = type_compare(&operandA->type, &operandB->type);

    if (result)
    {
        return result;
    }

    result = type_compare(&operandA->castAsType, &operandB->castAsType);

    if (result)
    {
        return result;
    }

    if ((operandA->permutation != VP_LITERAL && (operandB->permutation != VP_LITERAL)))
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

ssize_t tac_operand_compare(void *dataA, void *dataB)
{
    struct TACOperand *operandA = dataA;
    struct TACOperand *operandB = dataB;

    ssize_t result = tac_operand_compare_ignore_ssa_number(dataA, dataB);

    if (result != 0)
    {
        return result;
    }

    return ((ssize_t)operandA->ssaNumber - (ssize_t)operandB->ssaNumber);
}