#include "tac_operand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enum_desc.h"
#include "log.h"
#include "symtab_function.h"
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

    if (operand->permutation != VP_UNUSED)
    {
        char *nonCastTypeName = type_get_name(tac_operand_get_non_cast_type(operand));
        operandLen += sprintf(operandStr + operandLen, "%s", nonCastTypeName);
        free(nonCastTypeName);

        if ((operand->permutation == VP_STANDARD) || (operand->permutation == VP_TEMP))
        {
            if (operand->castAsType.basicType != VT_NULL)
            {
                char *castTypeName = type_get_name(&operand->castAsType);
                operandLen += sprintf(operandStr + operandLen, "(%s)", castTypeName);
                free(castTypeName);
            }
        }
        operandLen += sprintf(operandStr + operandLen, " ");
    }

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
        operandLen += sprintf(operandStr + operandLen, "0x%lx", operand->name.val);
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

    if (operandA->permutation != operandB->permutation)
    {
        return (ssize_t)operandA->permutation - (ssize_t)operandB->permutation;
    }

    switch (operandA->permutation)
    {
    case VP_STANDARD:
    case VP_TEMP:
        result = strcmp(operandA->name.variable->name, operandB->name.variable->name);
        break;

    case VP_LITERAL_STR:
        result = strcmp(operandA->name.str, operandB->name.str);
        break;

    case VP_LITERAL_VAL:
        result = (ssize_t)operandA->name.val - (ssize_t)operandB->name.val;
        break;

    case VP_UNUSED:
        result = 0;
        break;
    }

    return result;
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

void tac_operand_populate_as_temp(struct Scope *scope, struct TACOperand *operandToPopulate, struct Type *type)
{
    struct Type tempType = type_duplicate_non_pointer(type);
    if (scope->parentFunction == NULL)
    {
        InternalError("Attempt to create a temporary variable outside of a function scope\n");
    }
    char *tempName = temp_list_get(temps, (scope->parentFunction->tempNum)++);
    struct VariableEntry *tempVariable = scope_create_variable_by_name(scope, dictionary_lookup_or_insert(parseDict, tempName), &tempType, false, A_PUBLIC);
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
