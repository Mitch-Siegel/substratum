#include "linearizer.h"
#include "linearizer_opt0.h"

// given a raw size, find the nearest power-of-two aligned size
int alignSize(int nBytes)
{
	int i = 0;
	while ((nBytes > (0b1 << i)) > 0)
	{
		i++;
	}
	return i;
}

enum basicTypes selectVariableTypeForNumber(int num)
{
	if (num < 256)
	{
		return vt_uint8;
	}
	else if (num < 65536)
	{
		return vt_uint16;
	}
	else
	{
		return vt_uint32;
	}
}

enum basicTypes selectVariableTypeForLiteral(char *literal)
{
	int literalAsNumber = atoi(literal);
	enum basicTypes t = selectVariableTypeForNumber(literalAsNumber);
	return t;
}

void populateTACOperandFromVariable(struct TACOperand *o, struct VariableEntry *e)
{
	o->type = e->type;
	o->name.str = e->name;
	o->permutation = vp_standard;
}

extern struct TempList *temps;
extern struct Dictionary *parseDict;
struct TACLine *setUpScaleMultiplication(struct AST *tree, struct Scope *scope, int *TACIndex, int *tempNum, struct Type *pointerTypeOfToScale)
{
	struct TACLine *scaleMultiplication = newTACLine(*TACIndex, tt_mul, tree);

	scaleMultiplication->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	scaleMultiplication->operands[0].permutation = vp_temp;

	char scaleVal[32];
	snprintf(scaleVal, 31, "%d", Scope_getSizeOfDereferencedType(scope, pointerTypeOfToScale));
	scaleMultiplication->operands[2].name.str = Dictionary_LookupOrInsert(parseDict, scaleVal);
	scaleMultiplication->operands[2].permutation = vp_literal;
	scaleMultiplication->operands[2].type.basicType = vt_uint32;

	return scaleMultiplication;
}

struct SymbolTable *linearizeProgram(struct AST *program, int optimizationLevel)
{
    switch(optimizationLevel)
    {
        case 0:
            return walkProgram_0(program);

        default:
            ErrorAndExit(ERROR_INTERNAL, "Got invalid optimization level in linearizeProgram: %d\n", optimizationLevel);
    }
}