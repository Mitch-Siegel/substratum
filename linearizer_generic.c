#include "linearizer_generic.h"

#include "enum_desc.h"
#include "log.h"
#include "symtab_basicblock.h"
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

struct TACOperand *get_sizeof_type(struct Ast *tree,
                                   struct BasicBlock *block,
                                   struct Scope *scope,
                                   size_t *tacIndex,
                                   struct Type *getSizeof)
{
    struct TACLine *sizeofLine = new_tac_line(TT_SIZEOF, tree);
    struct TacSizeof *operands = &sizeofLine->operands.sizeof_;
    struct Type sizeType = {0};
    type_set_basic_type(&sizeType, VT_U64, NULL, 0);
    tac_operand_populate_as_temp(scope, &operands->destination, &sizeType);
    operands->type = type_duplicate_non_pointer(getSizeof);
    basic_block_append(block, sizeofLine, tacIndex);
    return &operands->destination;
}

struct TACLine *set_up_scale_multiplication(struct Ast *tree,
                                            struct BasicBlock *block,
                                            struct Scope *scope,
                                            size_t *TACIndex,
                                            struct Type *pointerTypeOfToScale,
                                            struct Type *offsetType)
{
    struct TACLine *scaleMultiplication = new_tac_line(TT_MUL, tree);

    tac_operand_populate_as_temp(scope, &scaleMultiplication->operands.arithmetic.destination, offsetType);

    struct Type dereferencedType = type_duplicate_non_pointer(pointerTypeOfToScale);
    dereferencedType.pointerLevel--;
    scaleMultiplication->operands.arithmetic.sourceB = *get_sizeof_type(tree, block, scope, TACIndex, &dereferencedType);
    type_deinit(&dereferencedType);

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

bool convert_array_load_to_lea(struct TACLine *loadLine, struct TACOperand *dest)
{
    bool changed = false;
    struct Type *loaded = tac_operand_get_type(&loadLine->operands.arrayLoad.destination);
    // if we have a load instruction, convert it to the corresponding lea instrutcion
    // leave existing lea instructions alone
    switch (loadLine->operation)
    {
    case TT_ARRAY_LOAD:
        loadLine->operation = TT_ARRAY_LEA;
        // increment indirection level as we just converted from a load to a lea
        // if pointing into a variable (such as in the case of temporaries), will update the type of the temp itself
        loaded->pointerLevel++;
        changed = true;
        break;

    case TT_ARRAY_LEA:
        break;

    default:
        InternalError("Unexpected TAC operation %s seen in convert_array_load_to_lea!", tac_operation_get_name(loadLine->operation));
        break;
    }

    // in case we are converting struct.member_which_is_struct.a, special case so that both operands guaranteed to have pointer type and thus be primitives for codegen
    if (loadLine->operands.arrayLoad.array.castAsType.basicType == VT_STRUCT)
    {
        loadLine->operands.arrayLoad.array.castAsType.pointerLevel++;
    }

    if (dest != NULL)
    {
        *dest = loadLine->operands.arrayLoad.destination;
    }

    return changed;
}

bool convert_field_load_to_lea(struct TACLine *loadLine, struct TACOperand *dest)
{
    bool changed = false;
    // if we have a load instruction, convert it to the corresponding lea instrutcion
    // leave existing lea instructions alone
    struct Type *loaded = tac_operand_get_type(&loadLine->operands.fieldLoad.destination);
    switch (loadLine->operation)
    {
    case TT_FIELD_LOAD:
        loadLine->operation = TT_FIELD_LEA;
        loaded->pointerLevel++;
        changed = true;
        break;

    case TT_FIELD_LEA:
        break;

    default:
        InternalError("Unexpected TAC operation %s seen in convert_field_load_to_lea!", tac_operation_get_name(loadLine->operation));
        break;
    }

    // in case we are converting struct.member_which_is_struct.a, special case so that both operands guaranteed to have pointer type and thus be primitives for codegen
    if (loadLine->operands.fieldLoad.source.castAsType.basicType == VT_STRUCT)
    {
        loadLine->operands.fieldLoad.source.castAsType.pointerLevel++;
    }

    if (dest != NULL)
    {
        *dest = loadLine->operands.fieldLoad.destination;
    }

    return changed;
}
