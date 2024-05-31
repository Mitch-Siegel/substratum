#include "linearizer_generic.h"

#include "log.h"
#include "symtab_variable.h"
#include "tac.h"
#include "type.h"
#include "util.h"
#include <stdlib.h>

enum basicTypes selectVariableTypeForNumber(size_t num)
{
    const size_t eightBitMax = 255;
    const size_t sixteenBitMax = 65535;

    enum basicTypes selectedType = vt_u32;
    if (num <= eightBitMax)
    {
        selectedType = vt_u8;
    }
    else if (num <= sixteenBitMax)
    {
        selectedType = vt_u16;
    }
    else
    {
        selectedType = vt_u32;
    }

    return selectedType;
}

enum basicTypes selectVariableTypeForLiteral(char *literal)
{
    // TODO: abstraction layer
    i32 literalAsNumber = atoi(literal);
    return selectVariableTypeForNumber(literalAsNumber);
}

void populateTACOperandFromVariable(struct TACOperand *operandToPopulate, struct VariableEntry *populateFrom)
{
    operandToPopulate->type = populateFrom->type;
    operandToPopulate->name.str = populateFrom->name;
    operandToPopulate->permutation = vp_standard;
}

extern struct TempList *temps;
void populateTACOperandAsTemp(struct TACOperand *operandToPopulate, size_t *tempNum)
{
    operandToPopulate->name.str = TempList_Get(temps, (*tempNum)++);
    operandToPopulate->permutation = vp_temp;
}

void copyTypeDecayArrays(struct Type *dest, struct Type *src)
{
    *dest = *src;
    Type_DecayArrays(dest);
}

void copyTACOperandDecayArrays(struct TACOperand *dest, struct TACOperand *src)
{
    *dest = *src;
    copyTACOperandTypeDecayArrays(dest, src);
}

void copyTACOperandTypeDecayArrays(struct TACOperand *dest, struct TACOperand *src)
{
    copyTypeDecayArrays(TACOperand_GetType(dest), TACOperand_GetType(src));
}

extern struct Dictionary *parseDict;
struct TACLine *setUpScaleMultiplication(struct AST *tree, struct Scope *scope, const size_t *TACIndex, size_t *tempNum, struct Type *pointerTypeOfToScale)
{
    struct TACLine *scaleMultiplication = newTACLine(tt_mul, tree);

    scaleMultiplication->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
    scaleMultiplication->operands[0].permutation = vp_temp;

    char scaleVal[sprintedNumberLength];
    snprintf(scaleVal, sprintedNumberLength - 1, "%zu", Type_GetSizeWhenDereferenced(pointerTypeOfToScale, scope));
    scaleMultiplication->operands[2].name.str = Dictionary_LookupOrInsert(parseDict, scaleVal);
    scaleMultiplication->operands[2].permutation = vp_literal;
    scaleMultiplication->operands[2].type.basicType = vt_u32;

    return scaleMultiplication;
}

void checkAccessedStructForDot(struct AST *tree, struct Scope *scope, struct Type *type)
{
    // check that we actually refer to a struct on the LHS of the dot
    if (type->basicType != vt_struct)
    {
        char *typeName = Type_GetName(type);
        // if we *are* looking at an identifier, print the identifier name and the type name
        if (tree->type == t_identifier)
        {
            LogTree(LOG_FATAL, tree, "Can't use dot operator on %s (%s) - not a struct!", tree->value, typeName);
        }
        // if we are *not* looking at an identifier, just print the type name
        else
        {
            LogTree(LOG_FATAL, tree, "Can't use dot operator on %s - not a struct!", typeName);
        }
    }

    if (type->pointerLevel > 1)
    {
        char *tooDeepPointerType = Type_GetName(type);
        LogTree(LOG_FATAL, tree, "Can't use dot operator on type %s - not a struct or struct pointer!", tooDeepPointerType);
    }
}

void convertLoadToLea(struct TACLine *loadLine, struct TACOperand *dest)
{
    // if we have a load instruction, convert it to the corresponding lea instrutcion
    // leave existing lea instructions alone
    switch (loadLine->operation)
    {
    case tt_load_arr:
        loadLine->operation = tt_lea_arr;
        break;

    case tt_load_off:
        loadLine->operation = tt_lea_off;
        break;

    case tt_lea_off:
    case tt_lea_arr:
        break;

    default:
        InternalError("Unexpected TAC operation %s seen in convertArrayRefLoadToLea!", getAsmOp(loadLine->operation));
        break;
    }

    // increment indirection level as we just converted from a load to a lea
    TAC_GetTypeOfOperand(loadLine, 0)->pointerLevel++;

    // in case we are converting struct.member_which_is_struct.a, special case so that both operands guaranteed to have pointer type and thus be primitives for codegen
    if (loadLine->operands[1].castAsType.basicType == vt_struct)
    {
        loadLine->operands[1].castAsType.pointerLevel++;
    }

    if (dest != NULL)
    {
        *dest = loadLine->operands[0];
    }
}
