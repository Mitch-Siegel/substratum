#include "tac_operand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab_enum.h"
#include "symtab_variable.h"
#include "util.h"

struct Type *tac_operand_get_type(struct TACOperand *operand)
{
    if ((operand->permutation == VP_UNUSED) || (operand->castAsType.basicType != VT_NULL))
    {
        return &operand->castAsType;
    }

    struct Type *gottenType = NULL;

    switch (operand->permutation)
    {
    case VP_STANDARD:
    case VP_TEMP:
        gottenType = &operand->name.variable->type;
        break;

    case VP_LITERAL_STR:
    case VP_LITERAL_VAL:
    case VP_UNUSED:
        gottenType = &operand->castAsType;
        break;
    }

    return gottenType;
}

struct Type *tac_operand_get_non_cast_type(struct TACOperand *operand)
{
    struct Type *nonCastType = NULL;

    switch (operand->permutation)
    {
    case VP_STANDARD:
    case VP_TEMP:
        nonCastType = &operand->name.variable->type;
        break;

    case VP_LITERAL_STR:
    case VP_LITERAL_VAL:
    case VP_UNUSED:
        nonCastType = &operand->castAsType;
        break;
    }

    return nonCastType;
}

const u8 TAC_OPERAND_NAME_LEN = 128;
char *tac_operand_sprint(void *operandData)
{
    struct TACOperand *operand = operandData;

    char *operandStr = malloc(TAC_OPERAND_NAME_LEN);
    ssize_t operandLen = 0;

    switch (operand->permutation)
    {
    case VP_STANDARD:
    case VP_TEMP:
        operandLen += sprintf(operandStr + operandLen, "%s", operand->name.variable->name);
        break;

    case VP_LITERAL_STR:
        operandLen += sprintf(operandStr + operandLen, "%s", operand->name.str);
        break;

    case VP_LITERAL_VAL:
        operandLen += sprintf(operandStr + operandLen, "%zu", operand->name.val);
        break;

    case VP_UNUSED:
        operandLen += sprintf(operandStr + operandLen, "UNUSED");
        break;
    }
    return operandStr;
}

ssize_t tac_operand_compare_ignore_ssa_number(void *dataA, void *dataB)
{
    struct TACOperand *operandA = dataA;
    struct TACOperand *operandB = dataB;

    ssize_t result = type_compare(tac_operand_get_non_cast_type(operandA), tac_operand_get_non_cast_type(operandB));

    if (result)
    {
        return result;
    }

    result = type_compare(&operandA->castAsType, &operandB->castAsType);

    if (result)
    {
        return result;
    }

    if (((operandA->permutation != VP_LITERAL_STR) && (operandA->permutation != VP_LITERAL_VAL)) &&
        ((operandB->permutation != VP_LITERAL_STR) && (operandB->permutation != VP_LITERAL_VAL)))
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
    operandToPopulate->name.variable = populateFrom;
    operandToPopulate->permutation = VP_STANDARD;
}

void tac_operand_populate_as_temp(struct Scope *scope, struct TACOperand *operandToPopulate, size_t *tempNum, struct Type *type)
{
    char *tempName = temp_list_get(temps, (*tempNum)++);
    struct VariableEntry *tempVariable = scope_create_variable_by_name(scope, dictionary_lookup_or_insert(parseDict, tempName), type, false, A_PUBLIC);
    operandToPopulate->permutation = VP_TEMP;
    operandToPopulate->name.variable = tempVariable;
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
