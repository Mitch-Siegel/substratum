#include "codegen.h"
#include "codegen_opt0.h"

char printedLine[MAX_ASM_LINE_SIZE];

char *registerNames[MACHINE_REGISTER_COUNT] = {
	"%r0",
	"%r1",
	"%r2",
	"%r3",
	"%r4",
	"%r5",
	"%r6",
	"%r7",
	"%r8",
	"%r9",
	"%r10",
	"%r11",
	"%r12",
	"%rr",
	"%sp",
	"%bp",
};

int ALIGNSIZE(unsigned int size)
{
	unsigned int nBits = 0;
	while (size)
	{
		nBits++;
		size >>= 1;
	}
	return nBits;
}

char *PlaceLiteralInRegister(FILE *outFile, char *literalStr, int destReg)
{
	char *destRegStr = registerNames[destReg];
	int literalValue = atoi(literalStr);
	if (literalValue < 0x100)
	{
		fprintf(outFile, "\tmovb %s, $%s ; place literal\n", destRegStr, literalStr);
	}
	else if (literalValue < 0x10000)
	{
		fprintf(outFile, "\tmovh %s, $%s ; place literal\n", destRegStr, literalStr);
	}
	else
	{
		int firstHalf, secondHalf;
		firstHalf = literalValue & 0xffff;
		secondHalf = literalValue >> 16;
		char halvedString[16]; // will be long enough to hold any int32/uint32 so can definitely hold half of one

		sprintf(halvedString, "%d", secondHalf);
		fprintf(outFile, "\tmovh %s, $%s\n", destRegStr, halvedString);

		fprintf(outFile, "\tshli %s, %s, $16\n", destRegStr, destRegStr);

		sprintf(halvedString, "%d", firstHalf);
		fprintf(outFile, "\taddi %s, %s, $%s ; place literal %s\n", destRegStr, destRegStr, halvedString, literalStr);
	}

	return destRegStr;
}

void verifyCodegenPrimitive(struct TACOperand *operand)
{
	struct Type *realType = TACOperand_GetType(operand);
	if (realType->basicType == vt_class)
	{
		if (realType->indirectionLevel == 0)
		{
			char *typeName = Type_GetName(realType);
			ErrorAndExit(ERROR_INTERNAL, "Error in verifyCodegenPrimitive: %s is not a primitive type!\n", typeName);
		}
	}
}

void WriteVariable(FILE *outFile,
				   struct LinkedList *lifetimes,
				   struct Scope *scope,
				   struct TACOperand *writtenTo,
				   int sourceRegIndex)
{
	verifyCodegenPrimitive(writtenTo);
	struct Lifetime *relevantLifetime = LinkedList_Find(lifetimes, compareLifetimes, writtenTo->name.str);
	if (relevantLifetime == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to find lifetime for variable %s!\n", writtenTo->name.str);
	}

	switch (relevantLifetime->wbLocation)
	{
	case wb_register:
		if (sourceRegIndex != relevantLifetime->registerLocation)
		{
			fprintf(outFile, "\t;Write register variable %s\n", relevantLifetime->name);
			fprintf(outFile, "\t%s %s, %s\n", SelectMovWidth(scope, writtenTo), registerNames[relevantLifetime->registerLocation], registerNames[sourceRegIndex]);
		}
		break;

	case wb_global:
	{
		const char *movOp = SelectMovWidth(scope, writtenTo);
		fprintf(outFile, "\t;Write (global) variable %s\n", relevantLifetime->name);
		fprintf(outFile, "\tmov %s, %s\n", registerNames[RETURN_REGISTER], relevantLifetime->name);
		fprintf(outFile, "\t%s (%s), %s\n", movOp, registerNames[RETURN_REGISTER], registerNames[sourceRegIndex]);
	}
	break;

	case wb_stack:
	{
		fprintf(outFile, "\t;Write stack variable %s\n", relevantLifetime->name);

		const char *movOp = SelectMovWidthForLifetime(scope, relevantLifetime);
		if (relevantLifetime->stackLocation >= 0)
		{
			fprintf(outFile, "\t%s (%%bp+%d), %s\n", movOp, relevantLifetime->stackLocation, registerNames[sourceRegIndex]);
		}
		else
		{
			fprintf(outFile, "\t%s (%%bp%d), %s\n", movOp, relevantLifetime->stackLocation, registerNames[sourceRegIndex]);
		}
	}
	break;

	case wb_unknown:
		ErrorAndExit(ERROR_INTERNAL, "Lifetime for %s has unknown writeback location!\n", relevantLifetime->name);
	}
}

// places an operand by name into the specified register, or returns the index of the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
int placeOrFindOperandInRegister(FILE *outFile,
								 struct Scope *scope,
								 struct LinkedList *lifetimes,
								 struct TACOperand *operand,
								 int registerIndex)
{
	verifyCodegenPrimitive(operand);

	/*
		TODO: Decide if this is too much of a hack?
		In cases where we have enough registers to not spill any loca variables, we still reserve 1 scratch register
		However, in cases where generic logic passes reserved[2] to this function, if that second operand is a global,
			reserved[2] will be -1 and we will not have anywhere to put the global, so select REGTURN_REGISTER instead
	*/

	if (operand->permutation == vp_literal)
	{
		if (registerIndex < 0)
		{
			ErrorAndExit(ERROR_INTERNAL, "Expected scratch register to place literal in, didn't get one!");
		}

		PlaceLiteralInRegister(outFile, operand->name.str, registerIndex);
		return registerIndex;
	}

	struct Lifetime *relevantLifetime = LinkedList_Find(lifetimes, compareLifetimes, operand->name.str);
	if (relevantLifetime == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to find lifetime for variable %s!\n", operand->name.str);
	}

	switch (relevantLifetime->wbLocation)
	{
	case wb_register:
		return relevantLifetime->registerLocation;

	case wb_global:
	{
		if (registerIndex == -1)
		{
			ErrorAndExit(ERROR_INTERNAL, "GOT -1 as register index to place operand in!\n");
			registerIndex = RETURN_REGISTER;
		}

		const char *movOp = SelectMovWidthForLifetime(scope, relevantLifetime);
		const char *usedRegister = registerNames[registerIndex];
		fprintf(outFile, "\tmov %s, %s ; place %s\n", usedRegister, relevantLifetime->name, operand->name.str);

		if (relevantLifetime->type.indirectionLevel == 0)
		{
			fprintf(outFile, "\t%s %s, (%s) ; place %s\n", movOp, usedRegister, usedRegister, operand->name.str);
		}
		return registerIndex;
	}
	break;

	case wb_stack:
	{
		if (registerIndex == -1)
		{
			ErrorAndExit(ERROR_INTERNAL, "GOT -1 as register index to place operand in!\n");
			registerIndex = RETURN_REGISTER;
		}

		const char *usedRegister = registerNames[registerIndex];
		if (relevantLifetime->type.arraySize > 0)
		{
			if (relevantLifetime->stackLocation >= 0)
			{
				fprintf(outFile, "\taddi %s, %%bp, $%d ; place %s\n", usedRegister, relevantLifetime->stackLocation, operand->name.str);
			}
			else
			{
				fprintf(outFile, "\tsubi %s, %%bp, $%d ; place %s\n", usedRegister, -1 * relevantLifetime->stackLocation, operand->name.str);
			}
		}
		else
		{
			const char *movOp = SelectMovWidthForLifetime(scope, relevantLifetime);
			if (relevantLifetime->stackLocation >= 0)
			{
				fprintf(outFile, "\t%s %s, (%%bp+%d) ; place %s\n", movOp, usedRegister, relevantLifetime->stackLocation, operand->name.str);
			}
			else
			{
				fprintf(outFile, "\t%s %s, (%%bp%d) ; place %s\n", movOp, usedRegister, relevantLifetime->stackLocation, operand->name.str);
			}
		}

		return registerIndex;
	}
	break;

	case wb_unknown:
	default:
		ErrorAndExit(ERROR_INTERNAL, "Lifetime for %s has unknown writeback location!\n", relevantLifetime->name);
	}
}

int pickWriteRegister(struct LinkedList *lifetimes,
					  struct Scope *scope,
					  struct TACOperand *operand,
					  int registerIndex)
{
	struct Lifetime *relevantLifetime = LinkedList_Find(lifetimes, compareLifetimes, operand->name.str);
	if (relevantLifetime == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to find lifetime for variable %s!\n", operand->name.str);
	}

	switch (relevantLifetime->wbLocation)
	{
	case wb_register:
		return relevantLifetime->registerLocation;

	case wb_stack:
	case wb_global:
		return registerIndex;

	case wb_unknown:
	default:
		ErrorAndExit(ERROR_INTERNAL, "Lifetime for %s has unknown writeback location!\n", relevantLifetime->name);
	}
}

const char *SelectMovWidthForSize(int size)
{
	switch (size)
	{
	case 1:
		return "movb";

	case 2:
		return "movh";

	case 4:
		return "mov";
	}
	ErrorAndExit(ERROR_INTERNAL, "Error in SelectMovWidth: Unexpected destination variable size\n\tVariable is not pointer, and is not of size 1, 2, or 4 bytes!");
}

const char *SelectMovWidth(struct Scope *scope, struct TACOperand *dataDest)
{
	// pointers are always full-width
	if (TACOperand_GetType(dataDest)->indirectionLevel > 0)
	{
		return "mov";
	}

	return SelectMovWidthForSize(Scope_getSizeOfType(scope, TACOperand_GetType(dataDest)));
}

const char *SelectMovWidthForDereference(struct Scope *scope, struct TACOperand *dataDest)
{
	struct Type *operandType = TACOperand_GetType(dataDest);
	if ((operandType->indirectionLevel == 0) &&
		(operandType->arraySize == 0))
	{
		ErrorAndExit(ERROR_INTERNAL, "SelectMovWidthForDereference called on non-indirect operand %s!\n", dataDest->name.str);
	}
	struct Type dereferenced = *operandType;
	if (operandType->indirectionLevel == 0)
	{
		operandType->arraySize = 0;
	}
	else
	{
		operandType->indirectionLevel--;
	}
	dereferenced.indirectionLevel--;
	dereferenced.arraySize = 0;
	return SelectMovWidthForSize(Scope_getSizeOfType(scope, &dereferenced));
}

const char *SelectPushWidthForSize(int size)
{
	switch (size)
	{
	case 1:
		return "pushb";

	case 2:
		return "pushh";

	case 4:
		return "push";
	}
	ErrorAndExit(ERROR_INTERNAL, "Error in SelectPushWidth: Unexpected destination variable size\n\tVariable is not pointer, and is not of size 1, 2, or 4 bytes!");
}

const char *SelectMovWidthForLifetime(struct Scope *scope, struct Lifetime *lifetime)
{
	if (lifetime->type.indirectionLevel > 0)
	{
		return "mov";
	}
	else
	{
		return SelectMovWidthForSize(Scope_getSizeOfType(scope, &lifetime->type));
	}
}

const char *SelectPushWidth(struct Scope *scope, struct TACOperand *dataDest)
{
	// pointers are always full-width
	if (TACOperand_GetType(dataDest)->indirectionLevel > 0)
	{
		return "push";
	}

	return SelectPushWidthForSize(Scope_getSizeOfType(scope, TACOperand_GetType(dataDest)));
}

void generateCode(struct SymbolTable *table, FILE *outFile, int regAllocOpt, int codegenOpt)
{
	switch (codegenOpt)
	{
	case 0:
		generateCodeForProgram_0(table, outFile, regAllocOpt);
		break;
	default:
		ErrorAndExit(ERROR_INTERNAL, "Got invalid optimization level in generateCode: %d\n", codegenOpt);
	}
}