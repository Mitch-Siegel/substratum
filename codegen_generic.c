#include "codegen_generic.h"

#include <stdarg.h>

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

void emitInstruction(struct TACLine *correspondingTACLine,
					 struct CodegenContext *c,
					 const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(c->outFile, format, args);
	va_end(args);

	if ((correspondingTACLine != NULL) && (correspondingTACLine->asmIndex == 0))
	{
		correspondingTACLine->asmIndex = (*(c->instructionIndex))++;
	}
}

char *PlaceLiteralStringInRegister(struct TACLine *correspondingTACLine,
								   struct CodegenContext *c,
								   char *literalStr,
								   int destReg)
{
	char *destRegStr = registerNames[destReg];
	emitInstruction(correspondingTACLine, c, "\tli %s, %s # place literal\n", destRegStr, literalStr);
	return destRegStr;
}

void verifyCodegenPrimitive(struct TACOperand *operand)
{
	struct Type *realType = TACOperand_GetType(operand);
	if (realType->basicType == vt_class)
	{
		if ((realType->indirectionLevel == 0) && (realType->arraySize == 0))
		{
			char *typeName = Type_GetName(realType);
			ErrorAndExit(ERROR_INTERNAL, "Error in verifyCodegenPrimitive: %s is not a primitive type!\n", typeName);
		}
	}
}

void WriteVariable(struct TACLine *correspondingTACLine,
				   struct CodegenContext *c,
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
			fprintf(c->outFile, "\t# Write register variable %s\n", relevantLifetime->name);
			emitInstruction(correspondingTACLine, c, "\tmv %s, %s\n",
							registerNames[relevantLifetime->registerLocation],
							registerNames[sourceRegIndex]);
		}
		break;

	case wb_global:
	{
		char width = SelectWidth(scope, writtenTo);
		fprintf(c->outFile, "\t# Write (global) variable %s\n", relevantLifetime->name);
		emitInstruction(correspondingTACLine, c, "\tla %s, %s\n",
						registerNames[TEMP_0],
						relevantLifetime->name);

		emitInstruction(correspondingTACLine, c, "\ts%c %s, 0(%s)\n",
						width,
						registerNames[sourceRegIndex],
						registerNames[TEMP_0]);
	}
	break;

	case wb_stack:
	{
		fprintf(c->outFile, "\t# Write stack variable %s\n", relevantLifetime->name);

		char width = SelectWidthForLifetime(scope, relevantLifetime);
		emitInstruction(correspondingTACLine, c, "\ts%c %s, %d(fp)\n",
						width,
						registerNames[sourceRegIndex],
						relevantLifetime->stackLocation);
	}
	break;

	case wb_unknown:
		ErrorAndExit(ERROR_INTERNAL, "Lifetime for %s has unknown writeback location!\n", relevantLifetime->name);
	}
}

// places an operand by name into the specified register, or returns the index of the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
int placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
								 struct CodegenContext *c,
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

		PlaceLiteralStringInRegister(correspondingTACLine, c, operand->name.str, registerIndex);
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

		char loadWidth = 'X';
		const char *loadSign = "";

		if (relevantLifetime->type.arraySize > 0)
		{
			// if array, treat as pointer
			loadWidth = 'd';
		}
		else
		{
			loadWidth = SelectWidthForLifetime(scope, relevantLifetime);
			loadSign = SelectSignForLoad(loadWidth, &relevantLifetime->type);
		}

		const char *usedRegister = registerNames[registerIndex];
		emitInstruction(correspondingTACLine, c, "\tla %s, %s # place %s\n",
						usedRegister,
						relevantLifetime->name,
						operand->name.str);

		if (relevantLifetime->type.arraySize == 0)
		{
			emitInstruction(correspondingTACLine, c, "\tl%c%s %s, 0(%s) # place %s\n",
							loadWidth,
							loadSign,
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
				emitInstruction(correspondingTACLine, c, "\taddi %s, fp, %d # place %s\n", usedRegister, relevantLifetime->stackLocation, operand->name.str);
			}
			else
			{
				emitInstruction(correspondingTACLine, c, "\taddi %s, fp, -%d # place %s\n", usedRegister, -1 * relevantLifetime->stackLocation, operand->name.str);
			}
		}
		else
		{
			char loadWidth = SelectWidthForLifetime(scope, relevantLifetime);
			emitInstruction(correspondingTACLine, c, "\tl%c%s %s, %d(fp) # place %s\n",
							loadWidth,
							SelectSignForLoad(loadWidth, &relevantLifetime->type),
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

int placeAddrOfLifetimeInReg(struct TACLine *correspondingTACLine,
							 struct CodegenContext *c,
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
		emitInstruction(correspondingTACLine, c, "\tla %s, %s\n", registerNames[registerIndex], relevantLifetime->name);
		return registerIndex; // early return since all that's necessary to place address of a global is the la instruction

	// stack variables are valid but require no action here, we emit an instruction calculating the offset next
	case wb_stack:
		break;

	case wb_unknown:
		ErrorAndExit(ERROR_INTERNAL, "placeAddrOfLifetimeInReg called on lifetime with unknown writeback location %s!\n", relevantLifetime->name);
		break;
	}

	if (relevantLifetime->stackLocation < 0)
	{
		emitInstruction(correspondingTACLine, c, "\taddi %s, fp, -%d\n", registerNames[registerIndex], -1 * relevantLifetime->stackLocation);
	}
	else
	{
		emitInstruction(correspondingTACLine, c, "\taddi %s, fp, -%d\n", registerNames[registerIndex], relevantLifetime->stackLocation);
	}

	return registerIndex;
}

char SelectWidthForSize(int size)
{
	switch (size)
	{
	case 1:
		return 'b';

	case 2:
		return 'h';

	case 4:
		return 'w';

	case 8:
		return 'd';
	}
	ErrorAndExit(ERROR_INTERNAL, "Error in SelectWidth: Unexpected destination variable size\n\tVariable is not pointer, and is not of size 1, 2, 4, or 8 bytes!");
}

const char *SelectSignForLoad(char loadSize, struct Type *loaded)
{
	switch (loadSize)
	{
	case 'b':
	case 'h':
	case 'w':
		return "u";

	case 'd':
		return "";

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected load size character seen in SelectSignForLoad!\n");
	}
}

char SelectWidth(struct Scope *scope, struct TACOperand *dataDest)
{
	// pointers and arrays (decay implicitly at this stage to pointers) are always full-width
	if ((TACOperand_GetType(dataDest)->indirectionLevel > 0) || (TACOperand_GetType(dataDest)->arraySize > 0))
	{
		return 'd';
	}

	return SelectWidthForSize(Scope_getSizeOfType(scope, TACOperand_GetType(dataDest)));
}

char SelectWidthForDereference(struct Scope *scope, struct TACOperand *dataDest)
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

char SelectWidthForLifetime(struct Scope *scope, struct Lifetime *lifetime)
{
	if (lifetime->type.indirectionLevel > 0)
	{
		return 'd';
	}
	else
	{
		return SelectWidthForSize(Scope_getSizeOfType(scope, &lifetime->type));
	}
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the frame pointer
void EmitFrameStoreForSize(struct TACLine *correspondingTACLine,
						   struct CodegenContext *c,
						   enum riscvRegisters sourceReg,
						   int size,
						   int offset)
{
	emitInstruction(correspondingTACLine, c, "\ts%c %s, %d(fp)\n", SelectWidthForSize(size), registerNames[sourceReg], offset);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the frame pointer
void EmitFrameLoadForSize(struct TACLine *correspondingTACLine,
						  struct CodegenContext *c,
						  enum riscvRegisters destReg,
						  int size,
						  int offset)
{
	emitInstruction(correspondingTACLine, c, "\tl%c %s, %d(fp)\n", SelectWidthForSize(size), registerNames[destReg], offset);
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the stack pointer
void EmitStackStoreForSize(struct TACLine *correspondingTACLine,
						   struct CodegenContext *c,
						   enum riscvRegisters sourceReg,
						   int size,
						   int offset)
{
	emitInstruction(correspondingTACLine, c, "\ts%c %s, %d(sp)\n", SelectWidthForSize(size), registerNames[sourceReg], offset);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the stack pointer
void EmitStackLoadForSize(struct TACLine *correspondingTACLine,
						  struct CodegenContext *c,
						  enum riscvRegisters sourceReg,
						  int size,
						  int offset)
{
	emitInstruction(correspondingTACLine, c, "\tl%c %s, %d(sp)\n", SelectWidthForSize(size), registerNames[sourceReg], offset);
}

void EmitPushForOperand(struct TACLine *correspondingTACLine,
						struct CodegenContext *c,
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
	case 8:
		EmitPushForSize(correspondingTACLine, c, size, srcRegister);

		break;

	default:
	{
		char *typeName = Type_GetName(TACOperand_GetType(dataSource));
		ErrorAndExit(ERROR_INTERNAL, "Unsupported size %d seen in EmitPushForOperand (for type %s)\n", size, typeName);
	}
	}
}

void EmitPushForSize(struct TACLine *correspondingTACLine,
					 struct CodegenContext *c,
					 int size,
					 int srcRegister)
{
	emitInstruction(correspondingTACLine, c, "\taddi sp, sp, -%d\n", size);
	emitInstruction(correspondingTACLine, c, "\ts%c %s, 0(sp)\n",
					SelectWidthForSize(size),
					registerNames[srcRegister]);
}

void EmitPopForOperand(struct TACLine *correspondingTACLine,
					   struct CodegenContext *c,
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
	case 8:
		EmitPopForSize(correspondingTACLine, c, size, destRegister);

		break;

	default:
	{
		char *typeName = Type_GetName(TACOperand_GetType(dataDest));
		ErrorAndExit(ERROR_INTERNAL, "Unsupported size %d seen in EmitPopForOperand (for type %s)\n", size, typeName);
	}
	}
}

void EmitPopForSize(struct TACLine *correspondingTACLine,
					struct CodegenContext *c,
					int size,
					int destRegister)
{
	emitInstruction(correspondingTACLine, c, "\tl%c%s %s, 0(sp)\n",
					SelectWidthForSize(size),
					(size == 8) ? "" : "u", // always generate an unsigned load (except for when loading 64 bit values, for which there is no unsigned load)
					registerNames[destRegister]);
	emitInstruction(correspondingTACLine, c, "\taddi sp, sp, %d\n", size);
}
