#include "codegen.h"
#include "codegen_opt0.h"

char printedLine[MAX_ASM_LINE_SIZE];

char *registerNames[MACHINE_REGISTER_COUNT] = {
	"zero",
	"ra",
	"sp",
	"gp",
	"tp",
	"t0",
	"t1",
	"t2",
	"fp",
	"s1",
	"a0",
	"a1",
	"a2",
	"a3",
	"a4",
	"a5",
	"a6",
	"a7",
	"s2",
	"s3",
	"s4",
	"s5",
	"s6",
	"s7",
	"s8",
	"s9",
	"s10",
	"s11",
	"t3",
	"t4",
	"t5",
	"t6",
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

char *PlaceLiteralStringInRegister(FILE *outFile, char *literalStr, int destReg)
{
	char *destRegStr = registerNames[destReg];
	fprintf(outFile, "\tli %s, %s # place literal\n", destRegStr, literalStr);
	return destRegStr;
}

char *PlaceLiteralInRegister(FILE *outFile, int literal, int destReg)
{
	char *destRegStr = registerNames[destReg];
	fprintf(outFile, "\tli %s, %d # place literal\n", destRegStr, literal);
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
				   struct Scope *scope,
				   struct LinkedList *lifetimes,
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
			fprintf(outFile, "\t# Write register variable %s\n", relevantLifetime->name);
			fprintf(outFile, "\tmv %s, %s\n",
					registerNames[relevantLifetime->registerLocation],
					registerNames[sourceRegIndex]);
		}
		break;

	case wb_global:
	{
		const char *width = SelectWidth(scope, writtenTo);
		fprintf(outFile, "\t# Write (global) variable %s\n", relevantLifetime->name);

		fprintf(outFile, "\tli %s, %s\n",
				registerNames[TEMP_0],
				relevantLifetime->name);

		fprintf(outFile, "\ts%s (%s), %s\n",
				width,
				registerNames[TEMP_0],
				registerNames[sourceRegIndex]);
	}
	break;

	case wb_stack:
	{
		fprintf(outFile, "\t# Write stack variable %s\n", relevantLifetime->name);

		const char *width = SelectWidthForLifetime(scope, relevantLifetime);
		fprintf(outFile, "\ts%s %d(fp), %s\n",
				width,
				relevantLifetime->stackLocation,
				registerNames[sourceRegIndex]);
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

	if (operand->permutation == vp_literal)
	{
		if (registerIndex < 0)
		{
			ErrorAndExit(ERROR_INTERNAL, "Expected scratch register to place literal in, didn't get one!");
		}

		PlaceLiteralStringInRegister(outFile, operand->name.str, registerIndex);
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
		}

		const char *loadWidth = NULL;

		if (relevantLifetime->type.arraySize > 0)
		{
			// if array, treat as pointer
			loadWidth = "wu";
		}
		else
		{
			loadWidth = SelectWidthForLifetime(scope, relevantLifetime);
		}
		const char *usedRegister = registerNames[registerIndex];
		fprintf(outFile, "\tmv %s, %s # place %s\n",
				usedRegister,
				relevantLifetime->name,
				operand->name.str);

		if (relevantLifetime->type.arraySize == 0)
		{
			fprintf(outFile, "\t%s %s, 0(%s) # place %s\n",
					loadWidth,
					usedRegister,
					usedRegister,
					operand->name.str);
		}

		return registerIndex;
	}
	break;

	case wb_stack:
	{
		if (registerIndex == -1)
		{
			ErrorAndExit(ERROR_INTERNAL, "GOT -1 as register index to place operand in!\n");
		}

		const char *usedRegister = registerNames[registerIndex];
		if (relevantLifetime->type.arraySize > 0)
		{
			if (relevantLifetime->stackLocation >= 0)
			{
				fprintf(outFile, "\taddi %s, fp, %d # place %s\n", usedRegister, relevantLifetime->stackLocation, operand->name.str);
			}
			else
			{
				fprintf(outFile, "\taddi %s, fp, -%d # place %s\n", usedRegister, -1 * relevantLifetime->stackLocation, operand->name.str);
			}
		}
		else
		{
			const char *loadWidth = SelectWidthForLifetime(scope, relevantLifetime);
			fprintf(outFile, "\tl%su %s, %d(fp) # place %s\n",
					loadWidth,
					usedRegister,
					relevantLifetime->stackLocation,
					operand->name.str);
		}

		return registerIndex;
	}
	break;

	case wb_unknown:
	default:
		ErrorAndExit(ERROR_INTERNAL, "Lifetime for %s has unknown writeback location!\n", relevantLifetime->name);
	}
}

int pickWriteRegister(struct Scope *scope,
					  struct LinkedList *lifetimes,
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

int placeAddrOfLifetimeInReg(FILE *outFile,
							 struct Scope *scope,
							 struct LinkedList *lifetimes,
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
		ErrorAndExit(ERROR_INTERNAL, "placeAddrOfLifetimeInReg called on register lifetime %s!\n", relevantLifetime->name);
		break;

	case wb_global:
	case wb_stack:
		break;

	case wb_unknown:
		ErrorAndExit(ERROR_INTERNAL, "placeAddrOfLifetimeInReg called on lifetime with unknown writeback location %s!\n", relevantLifetime->name);
		break;
	}

	if (relevantLifetime->stackLocation < 0)
	{
		fprintf(outFile, "\taddi %s, fp, -%d\n", registerNames[registerIndex], -1 * relevantLifetime->stackLocation);
	}
	else
	{
		fprintf(outFile, "\taddi %s, fp, -%d\n", registerNames[registerIndex], relevantLifetime->stackLocation);
	}

	return registerIndex;
}

const char *SelectWidthForSize(int size)
{
	switch (size)
	{
	case 1:
		return "b";

	case 2:
		return "h";

	case 4:
		return "w";
	}
	ErrorAndExit(ERROR_INTERNAL, "Error in SelectWidth: Unexpected destination variable size\n\tVariable is not pointer, and is not of size 1, 2, or 4 bytes!");
}

const char *SelectWidth(struct Scope *scope, struct TACOperand *dataDest)
{
	// pointers are always full-width
	if (TACOperand_GetType(dataDest)->indirectionLevel > 0)
	{
		return "wu";
	}

	return SelectWidthForSize(Scope_getSizeOfType(scope, TACOperand_GetType(dataDest)));
}

const char *SelectWidthForDereference(struct Scope *scope, struct TACOperand *dataDest)
{
	struct Type *operandType = TACOperand_GetType(dataDest);
	if ((operandType->indirectionLevel == 0) &&
		(operandType->arraySize == 0))
	{
		ErrorAndExit(ERROR_INTERNAL, "SelectWidthForDereference called on non-indirect operand %s!\n", dataDest->name.str);
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
	return SelectWidthForSize(Scope_getSizeOfType(scope, &dereferenced));
}

const char *SelectWidthForLifetime(struct Scope *scope, struct Lifetime *lifetime)
{
	if (lifetime->type.indirectionLevel > 0)
	{
		return "wu";
	}
	else
	{
		return SelectWidthForSize(Scope_getSizeOfType(scope, &lifetime->type));
	}
}

void EmitPushForOperand(FILE *outFile,
						struct Scope *scope,
						struct TACOperand *dataSource,
						int srcRegister)
{
	int size = Scope_getSizeOfType(scope, TACOperand_GetType(dataSource));
	switch (size)
	{
	case 1:
	case 2:
	case 4:
		EmitPushForSize(outFile, size, srcRegister);

		break;

	default:
		char *typeName = Type_GetName(TACOperand_GetType(dataSource));
		ErrorAndExit(ERROR_INTERNAL, "Unsupported size %d seen in EmitPushForOperand (for type %s)\n", size, typeName);
	}
}

void EmitPushForSize(FILE *outFile, int size, int srcRegister)
{
	fprintf(outFile, "\taddi sp, sp, -%d\n", size);
	fprintf(outFile, "\ts%s %s, 0(sp)\n",
			SelectWidthForSize(size),
			registerNames[srcRegister]);
}

void EmitPopForOperand(FILE *outFile,
							  struct Scope *scope,
							  struct TACOperand *dataDest,
							  int destRegister)
{
	int size = Scope_getSizeOfType(scope, TACOperand_GetType(dataDest));
	switch (size)
	{
	case 1:
	case 2:
	case 4:
		EmitPopForSize(outFile, size, destRegister);

		break;

	default:
		char *typeName = Type_GetName(TACOperand_GetType(dataDest));
		ErrorAndExit(ERROR_INTERNAL, "Unsupported size %d seen in EmitPopForOperand (for type %s)\n", size, typeName);
	}
}

void EmitPopForSize(FILE *outFile, int size, int destRegister)
{
	fprintf(outFile, "\tl%su %s, 0(sp)\n",
			SelectWidthForSize(size),
			registerNames[destRegister]);
	fprintf(outFile, "\taddi sp, sp, %d\n", size);
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