#include "codegen.h"

#define MAX_ASM_LINE_SIZE 256

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
		fprintf(outFile, "\tmovb %s, $%s\n", destRegStr, literalStr);
	}
	else if (literalValue < 0x10000)
	{
		fprintf(outFile, "\tmovh %s, $%s\n", destRegStr, literalStr);
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
		fprintf(outFile, "\taddi %s, %s, $%s\n", destRegStr, destRegStr, halvedString);
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
		fprintf(outFile, "\tmov %s, %s\n", usedRegister, relevantLifetime->name);

		if (relevantLifetime->type.indirectionLevel == 0)
		{
			fprintf(outFile, "\t%s %s, (%s)\n", movOp, usedRegister, usedRegister);
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
				fprintf(outFile, "\taddi %s, %%bp, $%d\n", usedRegister, relevantLifetime->stackLocation);
			}
			else
			{
				fprintf(outFile, "\tsubi %s, %%bp, $%d\n", usedRegister, -1 * relevantLifetime->stackLocation);
			}
		}
		else
		{
			const char *movOp = SelectMovWidthForLifetime(scope, relevantLifetime);
			if (relevantLifetime->stackLocation >= 0)
			{
				fprintf(outFile, "\t%s %s, (%%bp+%d)\n", movOp, usedRegister, relevantLifetime->stackLocation);
			}
			else
			{
				fprintf(outFile, "\t%s %s, (%%bp%d)\n", movOp, usedRegister, relevantLifetime->stackLocation);
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

int pickWriteRegister(struct LinkedList *lifetimes, struct Scope *scope, struct TACOperand *operand, int registerIndex)
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

// generates code from the global scope, recursing inwards to functions
void generateCode(struct SymbolTable *table, FILE *outFile)
{
	for (int i = 0; i < table->globalScope->entries->size; i++)
	{
		struct ScopeMember *thisMember = table->globalScope->entries->data[i];
		switch (thisMember->type)
		{
		case e_function:
		{
			struct FunctionEntry *generatedFunction = thisMember->entry;
			if (generatedFunction->isDefined)
			{
				fprintf(outFile, "~export funcdef %s\n", thisMember->name);
			}
			else
			{
				fprintf(outFile, "~export funcdec %s\n", thisMember->name);
			}

			char *returnType = Type_GetName(&generatedFunction->returnType);
			fprintf(outFile, "returns %s\n", returnType);
			free(returnType);

			fprintf(outFile, "%d arguments\n", generatedFunction->arguments->size);
			for (int j = 0; j < generatedFunction->arguments->size; j++)
			{
				struct VariableEntry *examinedArgument = generatedFunction->arguments->data[j];

				char *typeName = Type_GetName(&examinedArgument->type);
				fprintf(outFile, "%s %s\n", typeName, examinedArgument->name);
				free(typeName);
			}

			if (generatedFunction->isDefined)
			{
				generateCodeForFunction(outFile, generatedFunction);

				fprintf(outFile, "~end export funcdef %s\n", thisMember->name);
			}
			else
			{
				fprintf(outFile, "~end export funcdec %s\n", thisMember->name);
			}
		}
		break;

		case e_basicblock:
		{
			fprintf(outFile, "~export section userstart\n");
			struct LinkedList *globalBlockList = LinkedList_New();

			for (int j = 0; j < table->globalScope->entries->size; j++)
			{
				struct ScopeMember *m = table->globalScope->entries->data[j];
				if (m->type == e_basicblock)
				{
					LinkedList_Append(globalBlockList, m->entry);
				}
			}

			struct FunctionEntry start;
			start.argStackSize = 0;
			start.arguments = NULL;
			start.BasicBlockList = globalBlockList;
			start.mainScope = table->globalScope;
			start.name = "START";
			start.returnType.indirectionLevel = 0;
			start.returnType.basicType = vt_null;

			generateCodeForFunction(outFile, &start);
			LinkedList_Free(globalBlockList, NULL);
			fprintf(outFile, "~end export section userstart\n");
		}
		break;

		case e_variable:
		{
			struct VariableEntry *v = thisMember->entry;
			fprintf(outFile, "~export variable %s\n", thisMember->name);
			char *typeName = Type_GetName(&v->type);
			fprintf(outFile, "%s %s\n", typeName, v->name);
			free(typeName);
			if (v->type.initializeTo != NULL)
			{
				fprintf(outFile, "initialize\n");
				if (v->type.arraySize)
				{
					for (int e = 0; e < v->type.arraySize; e++)
					{
						fprintf(outFile, "#d8 ");
						for (int i = 0; i < Scope_getSizeOfArrayElement(table->globalScope, v); i++)
						{

							fprintf(outFile, "0x%02x ", v->type.initializeArrayTo[e][i]);
						}
						fprintf(outFile, "\n");
					}
				}
				else
				{
					fprintf(outFile, "#d8 ");
					for (int i = 0; i < Scope_getSizeOfType(table->globalScope, &v->type); i++)
					{
						fprintf(outFile, "0x%02x ", v->type.initializeTo[i]);
					}
					fprintf(outFile, "\n");
				}
			}
			else
			{
				fprintf(outFile, "noinitialize\n");
			}

			fprintf(outFile, "~end export variable %s\n", thisMember->name);
		}
		break;

		default:
			break;
		}
	}
};

/*
 * code generation for funcitons (lifetime management, etc)
 *
 */
void generateCodeForFunction(FILE *outFile, struct FunctionEntry *function)
{

	printf("generate code for function %s", function->name);

	struct CodegenMetadata metadata;
	memset(&metadata, 0, sizeof(struct CodegenMetadata));
	metadata.function = function;
	metadata.reservedRegisters[0] = -1;
	metadata.reservedRegisters[1] = -1;
	metadata.reservedRegisters[2] = -1;
	metadata.reservedRegisterCount = 0;
	int localStackSize = allocateRegisters(&metadata);

	printf("need %d bytes on stack :)\n", localStackSize);

	// emit function prologue
	fprintf(outFile, "%s:\n", function->name);

	if (localStackSize > 0)
	{
		fprintf(outFile, "\tsubi %%sp, %%sp, $%d\n", localStackSize);
	}

	for (int i = REGISTERS_TO_ALLOCATE - 1; i >= 0; i--)
	{
		if (metadata.touchedRegisters[i])
		{
			fprintf(outFile, "\tpush %%r%d\n", i);
		}
	}

	// move any applicable arguments into registers if we are expecting them not to be spilled
	for (struct LinkedListNode *ltRunner = metadata.allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
	{
		struct Lifetime *thisLifetime = ltRunner->data;

		// (short-circuit away from looking up temps since they can't be arguments)
		if (thisLifetime->name[0] != '.')
		{
			struct ScopeMember *thisEntry = Scope_lookup(function->mainScope, thisLifetime->name);
			// we need to place this variable into its register if:
			if ((thisEntry != NULL) &&							 // it exists
				(thisEntry->type == e_argument) &&				 // it's an argument
				(thisLifetime->wbLocation == wb_register) &&	 // it lives in a register
				(thisLifetime->nreads || thisLifetime->nwrites)) // theyre are either read from or written to at all
			{
				struct VariableEntry *theArgument = thisEntry->entry;

				const char *movOp = SelectMovWidthForLifetime(function->mainScope, thisLifetime);
				fprintf(outFile, "\t%s %%r%d, (%%bp+%d) ;place %s\n", movOp, thisLifetime->registerLocation, theArgument->stackOffset, thisLifetime->name);
			}
		}
	}

	// arguments placed into registers
	printf(".");

	for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
	{
		struct BasicBlock *block = blockRunner->data;
		GenerateCodeForBasicBlock(outFile, block, function->mainScope, metadata.allLifetimes, function->name, metadata.reservedRegisters);
	}

	// meaningful code generated
	printf(".");

	// emit function epilogue
	fprintf(outFile, "%s_done:\n", function->name);

	for (int i = 0; i < REGISTERS_TO_ALLOCATE; i++)
	{
		if (metadata.touchedRegisters[i])
		{
			fprintf(outFile, "\tpop %%r%d\n", i);
		}
	}

	if (localStackSize > 0)
	{
		fprintf(outFile, "\taddi %%sp, %%sp, $%d\n", localStackSize);
	}

	if (function->argStackSize > 0)
	{
		fprintf(outFile, "\tret %d\n", function->argStackSize);
	}
	else
	{
		fprintf(outFile, "\tret\n");
	}

	// function setup and teardown code generated
	printf(".");

	LinkedList_Free(metadata.allLifetimes, free);

	for (int i = 0; i <= metadata.largestTacIndex; i++)
	{
		LinkedList_Free(metadata.lifetimeOverlaps[i], NULL);
	}
	free(metadata.lifetimeOverlaps);

	printf("\n");
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
	if (dataDest->type.indirectionLevel > 0)
	{
		return "mov";
	}

	return SelectMovWidthForSize(Scope_getSizeOfType(scope, TACOperand_GetType(dataDest)));
}

const char *SelectMovWidthForDereference(struct Scope *scope, struct TACOperand *dataDestP)
{
	if (dataDestP->type.indirectionLevel < 1)
	{
		ErrorAndExit(ERROR_INTERNAL, "SelectMovWidthForDereference called on non-indirect operand %s!\n", dataDestP->name.str);
	}
	struct Type dereferenced = *TACOperand_GetType(dataDestP);
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
	if (dataDest->type.indirectionLevel > 0)
	{
		return "push";
	}

	return SelectPushWidthForSize(Scope_getSizeOfType(scope, TACOperand_GetType(dataDest)));
}

void GenerateCodeForBasicBlock(FILE *outFile,
							   struct BasicBlock *block,
							   struct Scope *scope,
							   struct LinkedList *lifetimes,
							   char *functionName,
							   int reservedRegisters[3])
{
	fprintf(outFile, "%s_%d:\n", functionName, block->labelNum);

	for (struct LinkedListNode *TACRunner = block->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
	{
		struct TACLine *thisTAC = TACRunner->data;

		if (thisTAC->operation != tt_asm)
		{
			char *printedTAC = sPrintTACLine(thisTAC);
			fprintf(outFile, "\n\t;%s\n", printedTAC);
			free(printedTAC);
		}

		switch (thisTAC->operation)
		{
		case tt_asm:
			fputs(thisTAC->operands[0].name.str, outFile);
			fputc('\n', outFile);
			break;

		case tt_assign:
		{
			// only works for primitive types that will fit in registers
			int opLocReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			WriteVariable(outFile, lifetimes, scope, &thisTAC->operands[0], opLocReg);
		}
		break;

		case tt_add:
		case tt_subtract:
		case tt_mul:
		case tt_div:
		{
			int op1Reg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int op2Reg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			int destReg = pickWriteRegister(lifetimes, scope, &thisTAC->operands[0], reservedRegisters[0]);

			fprintf(outFile, "\t%s %s, %s, %s\n", getAsmOp(thisTAC->operation), registerNames[destReg], registerNames[op1Reg], registerNames[op2Reg]);
			WriteVariable(outFile, lifetimes, scope, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_cmp:
		{
			int op1Reg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int op2Reg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);

			fprintf(outFile, "\t%s %s, %s\n", getAsmOp(thisTAC->operation), registerNames[op1Reg], registerNames[op2Reg]);
		}
		break;

		case tt_reference:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memw_1:
		{
			int destAddrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int sourceReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			const char *movOp = SelectMovWidth(scope, &thisTAC->operands[0]);
			fprintf(outFile, "\t%s (%s), %s\n", movOp, registerNames[destAddrReg], registerNames[sourceReg]);
		}
		break;

		case tt_memw_2:
		case tt_memw_2_n:
		{
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int sourceReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);

			const char *movOp = SelectMovWidth(scope, &thisTAC->operands[0]);

			if (thisTAC->operation == tt_memw_2)
			{
				fprintf(outFile, "\t%s (%s+%d), %s\n",
						movOp,
						registerNames[baseReg],
						thisTAC->operands[1].name.val,
						registerNames[sourceReg]);
			}
			else
			{
				fprintf(outFile, "\t%s (%s-%d), %s\n",
						movOp,
						registerNames[baseReg],
						thisTAC->operands[1].name.val,
						registerNames[sourceReg]);
			}
		}
		// ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
		break;

		case tt_memw_3:
		case tt_memw_3_n:
		{
			int curResIndex = 0;
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[curResIndex]);
			if (baseReg == reservedRegisters[curResIndex])
			{
				curResIndex++;
			}
			int offsetReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[curResIndex]);
			if (offsetReg == reservedRegisters[curResIndex])
			{
				curResIndex++;
			}
			int sourceReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[3], reservedRegisters[curResIndex]);
			if (sourceReg == reservedRegisters[curResIndex])
			{
				curResIndex++;
			}
			printf("curResIndex: %d - res = %d %d %d\n", curResIndex, reservedRegisters[0], reservedRegisters[1], reservedRegisters[2]);
			printf("Base: %d off: %d src: %d\n", baseReg, offsetReg, sourceReg);
			const char *movOp = SelectMovWidthForDereference(scope, &thisTAC->operands[0]);

			if (thisTAC->operation == tt_memw_3)
			{
				fprintf(outFile, "\t%s (%s+%s,%d), %s\n",
						movOp,
						registerNames[baseReg],
						registerNames[offsetReg],
						ALIGNSIZE(thisTAC->operands[2].name.val),
						registerNames[sourceReg]);
			}
			else
			{
				fprintf(outFile, "\t%s (%s-%s,%d), %s\n",
						movOp,
						registerNames[baseReg],
						registerNames[offsetReg],
						ALIGNSIZE(thisTAC->operands[2].name.val),
						registerNames[sourceReg]);
			}
		}
		break;

		case tt_dereference: // these are redundant... probably makes sense to remove one?
		case tt_memr_1:
		{
			int addrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			const char *movOp = SelectMovWidthForDereference(scope, &thisTAC->operands[1]);
			int destReg = pickWriteRegister(lifetimes, scope, &thisTAC->operands[0], reservedRegisters[0]);

			fprintf(outFile, "\t%s %s, (%s)\n",
					movOp,
					registerNames[destReg],
					registerNames[addrReg]);

			WriteVariable(outFile, lifetimes, scope, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_memr_2:
		case tt_memr_2_n:
		{
			int addrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int destReg = pickWriteRegister(lifetimes, scope, &thisTAC->operands[0], reservedRegisters[1]);
			const char *movOp = SelectMovWidth(scope, &thisTAC->operands[0]);
			if (thisTAC->operation == tt_memr_2)
			{
				fprintf(outFile, "\t%s %s, (%s+%d)\n",
						movOp,
						registerNames[destReg],
						registerNames[addrReg],
						thisTAC->operands[2].name.val);
			}
			else
			{
				fprintf(outFile, "\t%s %s, (%s-%d)\n",
						movOp,
						registerNames[destReg],
						registerNames[addrReg],
						thisTAC->operands[2].name.val);
			}

			WriteVariable(outFile, lifetimes, scope, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_memr_3:
		case tt_memr_3_n:
		{

			int addrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int offsetReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			int destReg = pickWriteRegister(lifetimes, scope, &thisTAC->operands[0], reservedRegisters[1]);
			const char *movOp = SelectMovWidthForDereference(scope, &thisTAC->operands[1]);
			if (thisTAC->operation == tt_memr_3)
			{
				fprintf(outFile, "\t%s %s, (%s+%s,%d)\n",
						movOp,
						registerNames[destReg],
						registerNames[addrReg],
						registerNames[offsetReg],
						ALIGNSIZE(thisTAC->operands[3].name.val));
			}
			else
			{
				fprintf(outFile, "\t%s %s, (%s-%s,%d)\n",
						movOp,
						registerNames[destReg],
						registerNames[addrReg],
						registerNames[offsetReg],
						ALIGNSIZE(thisTAC->operands[3].name.val));
			}
			WriteVariable(outFile, lifetimes, scope, &thisTAC->operands[0], destReg);
		}

		break;

		case tt_jg:
		case tt_jge:
		case tt_jl:
		case tt_jle:
		case tt_je:
		case tt_jne:
		case tt_jz:
		case tt_jnz:
		{
			fprintf(outFile, "\t%s %s_%d\n", getAsmOp(thisTAC->operation), functionName, thisTAC->operands[0].name.val);
		}
		break;

		case tt_jmp:
		{
			fprintf(outFile, "\tjmp %s_%d\n", functionName, thisTAC->operands[0].name.val);
		}
		break;

		case tt_push:
		{
			int operandRegister = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			fprintf(outFile, "\t%s %s\n", SelectPushWidth(scope, &thisTAC->operands[0]), registerNames[operandRegister]);
		}
		break;

		case tt_call:
		{
			fprintf(outFile, "\tcall %s\n", thisTAC->operands[1].name.str);

			if (thisTAC->operands[0].name.str != NULL)
			{
				WriteVariable(outFile, lifetimes, scope, &thisTAC->operands[0], RETURN_REGISTER);
			}
		}
		break;

		case tt_label:
			fprintf(outFile, "\t%s_%d:\n", functionName, thisTAC->operands[0].name.val);
			break;

		case tt_return:
		{
			if (thisTAC->operands[0].name.str != NULL)
			{
				int sourceReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], RETURN_REGISTER);

				if (sourceReg != RETURN_REGISTER)
				{
					fprintf(outFile, "\t%s %s, %s\n",
							SelectMovWidth(scope, &thisTAC->operands[0]),
							registerNames[RETURN_REGISTER],
							registerNames[sourceReg]);
				}
			}
			fprintf(outFile, "jmp %s_done\n", scope->parentFunction->name);
		}
		break;

		case tt_do:
		case tt_enddo:
			break;
		}
	}
}
