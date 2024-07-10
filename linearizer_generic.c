#include "linearizer_generic.h"

#include "log.h"
#include "symtab_enum.h"
#include "symtab_variable.h"
#include "tac.h"
#include "type.h"
#include "util.h"
#include <stdlib.h>

enum BASIC_TYPES select_variable_type_for_number(size_t num)
{
    const size_t EIGHT_BIT_MAX = 255;
    const size_t SIXTEEN_BIT_MAX = 65535;

    enum BASIC_TYPES selectedType = VT_U32;
    if (num <= EIGHT_BIT_MAX)
    {
        selectedType = VT_U8;
    }
    else if (num <= SIXTEEN_BIT_MAX)
    {
        selectedType = VT_U16;
    }
    else
    {
        selectedType = VT_U32;
    }

    return selectedType;
}

enum BASIC_TYPES select_variable_type_for_literal(char *literal)
{
    // TODO: abstraction layer
    i32 literalAsNumber = atoi(literal);
    return select_variable_type_for_number(literalAsNumber);
}

extern struct Dictionary *parseDict;

extern struct TempList *temps;

struct TACLine *set_up_scale_multiplication(struct Ast *tree, struct Scope *scope, const size_t *TACIndex, size_t *tempNum, struct Type *pointerTypeOfToScale)
{
    struct TACLine *scaleMultiplication = new_tac_line(TT_MUL, tree);

    scaleMultiplication->operands[0].name.str = temp_list_get(temps, (*tempNum)++);
    scaleMultiplication->operands[0].permutation = VP_TEMP;

    char scaleVal[sprintedNumberLength];
    snprintf(scaleVal, sprintedNumberLength - 1, "%zu", type_get_size_when_dereferenced(pointerTypeOfToScale, scope));
    scaleMultiplication->operands[2].name.str = dictionary_lookup_or_insert(parseDict, scaleVal);
    scaleMultiplication->operands[2].permutation = VP_LITERAL;
    scaleMultiplication->operands[2].type.basicType = VT_U32;

    return scaleMultiplication;
}

void check_accessed_struct_for_dot(struct Ast *tree, struct Scope *scope, struct Type *type)
{
    // check that we actually refer to a struct on the LHS of the dot
    if (type->basicType != VT_STRUCT)
    {
        char *typeName = type_get_name(type);
        // if we *are* looking at an identifier, print the identifier name and the type name
        if (tree->type == T_IDENTIFIER)
        {
            log_tree(LOG_FATAL, tree, "Can't use dot operator on %s (%s) - not a struct!", tree->value, typeName);
        }
        // if we are *not* looking at an identifier, just print the type name
        else
        {
            log_tree(LOG_FATAL, tree, "Can't use dot operator on %s - not a struct!", typeName);
        }
    }

    if (type->pointerLevel > 1)
    {
        char *tooDeepPointerType = type_get_name(type);
        log_tree(LOG_FATAL, tree, "Can't use dot operator on type %s - not a struct or struct pointer!", tooDeepPointerType);
    }
}

void convert_array_load_to_lea(struct TACLine *loadLine, struct TACOperand *dest)
{
    // if we have a load instruction, convert it to the corresponding lea instrutcion
    // leave existing lea instructions alone
    switch (loadLine->operation)
    {
    case TT_ARRAY_LOAD:
        loadLine->operation = TT_ARRAY_LEA;
        break;

    case TT_ARRAY_LEA:
        break;

    default:
        InternalError("Unexpected TAC operation %s seen in convert_array_load_to_lea!", get_asm_op(loadLine->operation));
        break;
    }

    struct Type *loaded = tac_get_type_of_operand(loadLine, 0);
    // increment indirection level as we just converted from a load to a lea
    if (loaded->basicType != VT_ARRAY)
    {
        loaded->pointerLevel++;
    }
    else
    {
        type_single_decay(loaded);
    }

    // in case we are converting struct.member_which_is_struct.a, special case so that both operands guaranteed to have pointer type and thus be primitives for codegen
    if (loadLine->operands[1].castAsType.basicType == VT_STRUCT)
    {
        loadLine->operands[1].castAsType.pointerLevel++;
    }

    if (dest != NULL)
    {
        *dest = loadLine->operands[0];
    }
}

void convert_field_load_to_lea(struct TACLine *loadLine, struct TACOperand *dest)
{
    // if we have a load instruction, convert it to the corresponding lea instrutcion
    // leave existing lea instructions alone
    struct Type *loaded = tac_get_type_of_operand(loadLine, 0);
    switch (loadLine->operation)
    {
    case TT_FIELD_LOAD:
        loadLine->operation = TT_FIELD_LEA;
        loaded->pointerLevel++;
        break;

    case TT_FIELD_LEA:
        break;

    default:
        InternalError("Unexpected TAC operation %s seen in convert_field_load_to_lea!", get_asm_op(loadLine->operation));
        break;
    }

    // in case we are converting struct.member_which_is_struct.a, special case so that both operands guaranteed to have pointer type and thus be primitives for codegen
    if (loadLine->operands[1].castAsType.basicType == VT_STRUCT)
    {
        loadLine->operands[1].castAsType.pointerLevel++;
    }

    if (dest != NULL)
    {
        *dest = loadLine->operands[0];
    }
}
