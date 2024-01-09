#include "linearizer_generic.h"

enum basicTypes selectVariableTypeForNumber(int num)
{
	if (num < 256)
	{
		return vt_u8;
	}
	else if (num < 65536)
	{
		return vt_u16;
	}
	else
	{
		return vt_u32;
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

void copyTypeDecayArrays(struct Type *dest, struct Type *src)
{
	*dest = *src;
	if (dest->arraySize > 0)
	{
		dest->arraySize = 0;
		dest->indirectionLevel++;
	}
}

void copyTACOperandDecayArrays(struct TACOperand *dest, struct TACOperand *src)
{
	*dest = *src;
	copyTACOperandTypeDecayArrays(dest, src);
}

void copyTACOperandTypeDecayArrays(struct TACOperand *dest, struct TACOperand *src)
{
	copyTypeDecayArrays(&dest->type, &src->type);
	copyTypeDecayArrays(&dest->castAsType, &src->castAsType);
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
	scaleMultiplication->operands[2].type.basicType = vt_u32;

	return scaleMultiplication;
}

// check the type of an AST, return true if mismatch
char ensureASTType(struct AST *tree, enum token type)
{
	return !(tree->type == type);
}

// check the LHS of any dot operator make sure it is both a class and not indirect
// special case handling for when tree is an identifier vs a subexpression
void checkAccessedClassForDot(struct AST *tree, struct Scope *scope, struct Type *type)
{
	// check that we actually refer to a class on the LHS of the dot
	if (type->basicType != vt_class)
	{
		char *typeName = Type_GetName(type);
		// if we *are* looking at an identifier, print the identifier name and the type name
		if (!ensureASTType(tree, t_identifier))
		{
			ErrorWithAST(ERROR_CODE, tree, "Can't use dot operator on %s (%s) - not a class!\n", tree->value, typeName);
		}
		// if we are *not* looking at an identifier, just print the type name
		else
		{
			ErrorWithAST(ERROR_CODE, tree, "Can't use dot operator on %s - not a class!\n", typeName);
		}
	}

	// make sure whatever we're applying the dot operator to is actually a class instance, not a class array or class pointer
	if ((type->indirectionLevel > 0) || (type->arraySize > 0))
	{
		char *typeName = Type_GetName(type);
		if (!ensureASTType(tree, t_identifier))
		{
			ErrorWithAST(ERROR_CODE, tree, "Can't use dot operator on indirect variable %s (%s)\n", tree->value, typeName);
		}
		else
		{
			ErrorWithAST(ERROR_CODE, tree, "Can't use dot operator on indirect type %s\n", typeName);
		}
	}
}

// check the LHS of any arrow operator, make sure it is only a class pointer and nothing else
// special case handling for when tree is an identifier vs a subexpression
void checkAccessedClassForArrow(struct AST *tree, struct Scope *scope, struct Type *type)
{
	// check that we actually refer to a class on the LHS of the arrow
	if (type->basicType != vt_class)
	{
		char *typeName = Type_GetName(type);
		// if we *are* looking at an identifier, print the identifier name and the type name
		if (!ensureASTType(tree, t_identifier))
		{
			ErrorWithAST(ERROR_CODE, tree, "Can't use arrow operator on %s (type %s) - not a class!\n", tree->value, typeName);
		}
		// if we are *not* looking at an identifier, just print the type name
		else
		{
			ErrorWithAST(ERROR_CODE, tree, "Can't use arrow operator on %s - not a class!\n", typeName);
		}
	}

	// make sure whatever we're applying the arrow operator to is actually a class pointer instance and nothing else
	if ((type->indirectionLevel != 1) || (type->arraySize > 0))
	{
		char *typeName = Type_GetName(type);
		if (!ensureASTType(tree, t_identifier))
		{
			ErrorWithAST(ERROR_CODE, tree, "Can't use arrow operator on variable %s (type %s) - wrong indirection level!\n", tree->value, typeName);
		}
		else
		{
			ErrorWithAST(ERROR_CODE, tree, "Can't use arrow operator on indirect type %s - wrong indirection level!\n", typeName);
		}
	}
}
