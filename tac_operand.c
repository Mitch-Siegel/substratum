#include "tac_operand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab_variable.h"
#include "symtab_enum.h"
#include "util.h"

struct Type *tac_operand_get_type(struct TACOperand *operand)
{
    if (operand->castAsType.basicType != VT_NULL)
    {
        return &operand->castAsType;
    }

    return &operand->type;
}

void tac_operand_print(void *operandData)
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

extern struct Dictionary *parseDict;
extern struct TempList *temps;

void tac_operand_populate_from_variable(struct TACOperand *operandToPopulate, struct VariableEntry *populateFrom)
{
    type_init(&operandToPopulate->castAsType);
    operandToPopulate->type = populateFrom->type;
    operandToPopulate->name.str = populateFrom->name;
    operandToPopulate->permutation = VP_STANDARD;
}

void tac_operand_populate_from_enum_member(struct TACOperand *operandToPopulate, struct EnumEntry *theEnum, struct Ast *tree)
{
    type_init(&operandToPopulate->castAsType);
    type_init(&operandToPopulate->type);
    operandToPopulate->type.basicType = VT_ENUM;
    operandToPopulate->permutation = VP_LITERAL;

    struct EnumMember *member = enum_lookup_member(theEnum, tree);

    operandToPopulate->type.nonArray.complexType.name = theEnum->name;

    char enumAsLiteral[sprintedNumberLength];
    snprintf(enumAsLiteral, sprintedNumberLength - 1, "%zu", member->numerical);
    operandToPopulate->name.str = dictionary_lookup_or_insert(parseDict, enumAsLiteral);
}

void tac_operand_populate_as_temp(struct TACOperand *operandToPopulate, size_t *tempNum)
{
    operandToPopulate->name.str = temp_list_get(temps, (*tempNum)++);
    operandToPopulate->permutation = VP_TEMP;
}

void tac_operand_copy_decay_arrays(struct TACOperand *dest, struct TACOperand *src)
{
    *dest = *src;
    tac_operand_copy_type_decay_arrays(dest, src);
}

void tac_operand_copy_type_decay_arrays(struct TACOperand *dest, struct TACOperand *src)
{
    type_copy_decay_arrays(tac_operand_get_type(dest), tac_operand_get_type(src));
}
