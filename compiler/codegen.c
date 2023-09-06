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

void WriteSpilledVariable(FILE *outFile, struct Scope *scope, struct Lifetime *writtenTo, char *sourceRegStr)
{
	// if we have a global, we will use a string reference to the variable's name for the linker to resolve once it places globals
	if (writtenTo->isGlobal)
	{
		// we will need 2 movs because the address of the global will be a 32-bit int, requiring an assembly macro of its own
		fprintf(outFile, "\tmov %s, %s\n", registerNames[RETURN_REGISTER], writtenTo->name);
		// then, once we have the address in a register we can write our value to it
		fprintf(outFile, "\t%s (%s), %s\n", SelectMovWidthForLifetime(scope, writtenTo), registerNames[RETURN_REGISTER], sourceRegStr);
	}
	// not a global
	else
	{
		// location is a positive offset, we will add to the base pointer to get to this variable
		if (writtenTo->stackOrRegLocation > 0)
		{
			fprintf(outFile, "\t%s (%%bp+%d), %s\n", SelectMovWidthForLifetime(scope, writtenTo), writtenTo->stackOrRegLocation, sourceRegStr);
		}
		// location is a negative offset, we will subtract from the base pointer to get to this variable
		else
		{
			fprintf(outFile, "\t%s (%%bp%d), %s\n", SelectMovWidthForLifetime(scope, writtenTo), writtenTo->stackOrRegLocation, sourceRegStr);
		}
	}
}

char *ReadSpilledVariable(FILE *outFile, struct Scope *scope, int destReg, struct Lifetime *readFrom)
{
	char *destRegStr = registerNames[destReg];

	// if we have a global, we will use a string reference to the variable's name for the linker to resolve once it places globals
	if (readFrom->isGlobal)
	{
		fprintf(outFile, "\t; read global %s\n", readFrom->name);
		// we will need 2 movs because the address of the global will be a 32-bit int, requiring an assembly macro of its own
		fprintf(outFile, "\tmov %s, %s\n", destRegStr, readFrom->name);
		// then, once we have the address in a register we can read from it to grab our value
		fprintf(outFile, "\t%s %s, (%s)\n", SelectMovWidthForLifetime(scope, readFrom), destRegStr, destRegStr);
	}
	// not a global
	else
	{
		fprintf(outFile, "\t; read spilled variable %s\n", readFrom->name);
		// location is a positive offset, we will add to the base pointer to get to this variable
		if (readFrom->stackOrRegLocation > 0)
		{
			fprintf(outFile, "\t%s %s, (%%bp+%d)\n", SelectMovWidthForLifetime(scope, readFrom), destRegStr, readFrom->stackOrRegLocation);
		}
		// location is a negative offset, we will subtract from the base pointer to get to this variable
		else
		{
			fprintf(outFile, "\t%s %s, (%%bp%d)\n", SelectMovWidthForLifetime(scope, readFrom), destRegStr, readFrom->stackOrRegLocation);
		}
	}
	return destRegStr;
}

// places an operand by name into the specified register, or returns the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
char *placeOrFindOperandInRegister(struct LinkedList *lifetimes, struct Scope *scope, struct TACOperand operand, FILE *outFile, int registerIndex, char *touchedRegisters)
{
	/*
		TODO: Decide if this is too much of a hack?
		In cases where we have enough registers to not spill any loca variables, we still reserve 1 scratch register
		However, in cases where generic logic passes reserved[2] to this function, if that second operand is a global,
			reserved[2] will be -1 and we will not have anywhere to put the global, so select REGTURN_REGISTER instead
	*/
	if (registerIndex == -1)
	{
		registerIndex = RETURN_REGISTER;
	}

	if (operand.permutation == vp_literal)
	{
		if (registerIndex < 0)
		{
			ErrorAndExit(ERROR_INTERNAL, "Expected scratch register to place literal in, didn't get one!");
		}

		return PlaceLiteralInRegister(outFile, operand.name.str, registerIndex);
	}

	if (operand.permutation == vp_objptr)
	{
		fprintf(outFile, "\t%s %s, %s\n", SelectMovWidth(scope, &operand), registerNames[registerIndex], operand.name.str);
		return registerNames[registerIndex];
	}

	struct Lifetime *relevantLifetime = LinkedList_Find(lifetimes, compareLifetimes, operand.name.str);
	if (relevantLifetime == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to find lifetime for variable %s!\n", operand.name.str);
	}

	if (registerIndex < 0 && relevantLifetime->isSpilled && !relevantLifetime->isGlobal)
	{
		ErrorAndExit(ERROR_INTERNAL, "Call to attempt to place spilled variable %s when none should be spilled!", operand.name.str);
	}
	// else the variable is in a register already

	// if not a local pointer, the value for this variable *must* exist either in a register or spilled on the stack
	if (relevantLifetime->localPointerTo == NULL)
	{
		if (relevantLifetime->isSpilled)
		{
			char *resultRegisterStr = ReadSpilledVariable(outFile, scope, registerIndex, relevantLifetime);
			touchedRegisters[registerIndex] = 1;
			return resultRegisterStr;
		}
		else
		{
			return registerNames[relevantLifetime->stackOrRegLocation];
		}
	}
	else
	{
		// if this local pointer doesn't live in a register, we will need to construct it on demand
		if (relevantLifetime->isSpilled)
		{
			char *destRegStr = registerNames[registerIndex];
			int basepointerOffset = relevantLifetime->localPointerTo->stackOffset;
			if (basepointerOffset > 0)
			{
				fprintf(outFile, "\taddi %s, %%bp, $%d\n", destRegStr, basepointerOffset);
			}
			else if (basepointerOffset < 0)
			{
				fprintf(outFile, "\tsubi %s, %%bp, $%d\n", destRegStr, -1 * basepointerOffset);
			}
			else
			{
				fprintf(outFile, "\tmov %s, %%bp\n", destRegStr);
			}
			touchedRegisters[registerIndex] = 1;
			return destRegStr;
		}
		// if it does get a register, all we need to do is return it
		else
		{
			return registerNames[relevantLifetime->stackOrRegLocation];
		}
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
				generateCodeForFunction(generatedFunction, outFile);

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
			start.localStackSize = 0;
			start.mainScope = table->globalScope;
			start.name = "START";
			start.returnType.indirectionLevel = 0;
			start.returnType.basicType = vt_null;

			generateCodeForFunction(&start, outFile);
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
			fprintf(outFile, "~end export variable %s\n", thisMember->name);
		}
		break;

		case e_object:
		{
			struct ObjectEntry *o = thisMember->entry;
			fprintf(outFile, "~export object %s\n", thisMember->name);
			fprintf(outFile, "size %d initialized %d\n", o->size, o->initialized);
			if (o->initialized)
			{
				for (int j = 0; j < o->size; j++)
				{
					fprintf(outFile, "%02x ", o->initializeTo[j]);
				}
				fprintf(outFile, "\n");
			}
			fprintf(outFile, "~end export object %s\n", thisMember->name);
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
void generateCodeForFunction(struct FunctionEntry *function, FILE *outFile)
{
	printf("generate code for function %s", function->name);

	struct CodegenMetadata metadata;
	metadata.function = function;
	metadata.allLifetimes = findLifetimes(function->mainScope, function->BasicBlockList);

	// find all overlapping lifetimes, to figure out which variables can live in registers vs being spilled
	metadata.largestTacIndex = 0;
	for (struct LinkedListNode *runner = metadata.allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		if (thisLifetime->end > metadata.largestTacIndex)
		{
			metadata.largestTacIndex = thisLifetime->end;
		}
	}

	// generate an array of lists corresponding to which lifetimes are active at a given TAC step by index in the array
	metadata.lifetimeOverlaps = malloc((metadata.largestTacIndex + 1) * sizeof(struct LinkedList *));
	for (int i = 0; i <= metadata.largestTacIndex; i++)
	{
		metadata.lifetimeOverlaps[i] = LinkedList_New();
	}

	metadata.spilledLifetimes = Stack_New();
	metadata.localPointerLifetimes = Stack_New();

	metadata.reservedRegisters[0] = 0;
	metadata.reservedRegisters[1] = -1;
	metadata.reservedRegisters[2] = -1;

	int mostConcurrentLifetimes = generateLifetimeOverlaps(&metadata);

	// printf("\n");
	// printf("at most %d concurrent lifetimes\n", mostConcurrentLifetimes);

	spillVariables(&metadata, mostConcurrentLifetimes);

	sortSpilledLifetimes(&metadata);

	// find the total size of the function's stack frame containing local variables *and* spilled variables
	int stackOffset = function->localStackSize; // start with just the things guaranteed to be on the local stack
	for (int i = 0; i < metadata.spilledLifetimes->size; i++)
	{
		struct Lifetime *thisLifetime = metadata.spilledLifetimes->data[i];
		int thisSize = Scope_getSizeOfType(function->mainScope, &thisLifetime->type);

		// if the variable isn't a local pointer (can calculate local pointers on the fly from base pointer so no need to store)
		if (thisLifetime->localPointerTo == NULL)
		{
			struct ScopeMember *thisVariableEntry = Scope_lookup(function->mainScope, thisLifetime->name);
			char stackSlotExists = 0;
			if (thisVariableEntry != NULL)
			{
				if (thisVariableEntry->type == e_argument)
				{
					struct VariableEntry *thisVariable = thisVariableEntry->entry;
					thisLifetime->stackOrRegLocation = thisVariable->stackOffset;
					stackSlotExists = 1;
				}
			}

			if (!stackSlotExists)
			{
				// constant offset of -2 for return address
				stackOffset += thisSize;
				thisLifetime->stackOrRegLocation = -1 * stackOffset;
			}
		}
	}

	assignRegisters(&metadata);

	// actual registers have been assigned to variables
	printf(".");

	// emit function prologue
	fprintf(outFile, "%s:\n", function->name);

	if (stackOffset > 0)
	{
		fprintf(outFile, "\tsubi %%sp, %%sp, $%d\n", stackOffset);
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
				(!thisLifetime->isSpilled) &&					 // they're not spilled
				(thisLifetime->nreads || thisLifetime->nwrites)) // theyre are either read from or written to at all
			{
				struct VariableEntry *theArgument = thisEntry->entry;
				const char *movOp = SelectMovWidthForLifetime(function->mainScope, thisLifetime);
				fprintf(outFile, "\t%s %%r%d, (%%bp+%d) ;place %s\n", movOp, thisLifetime->stackOrRegLocation, theArgument->stackOffset, thisLifetime->name);
				metadata.touchedRegisters[thisLifetime->stackOrRegLocation] = 1;
			}
		}
	}

	// arguments placed into registers
	printf(".");

	for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
	{
		struct BasicBlock *thisBlock = blockRunner->data;
		GenerateCodeForBasicBlock(thisBlock, function->mainScope, metadata.allLifetimes, function->name, metadata.reservedRegisters, metadata.touchedRegisters, outFile);
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

	if (stackOffset > 0)
	{
		fprintf(outFile, "\taddi %%sp, %%sp, $%d\n", stackOffset);
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

	Stack_Free(metadata.spilledLifetimes);
	Stack_Free(metadata.localPointerLifetimes);
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

void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock,
							   struct Scope *thisScope,
							   struct LinkedList *allLifetimes,
							   char *functionName,
							   int reservedRegisters[3],
							   char *touchedRegisters,
							   FILE *outFile)
{
	// TODO: generate localpointers as necessary
	// to registers for those that get them and on-the-fly for those that don't
	if (thisBlock->labelNum > 0)
	{
		fprintf(outFile, "%s_%d:\n", functionName, thisBlock->labelNum);
	}
	else
	{
		fprintf(outFile, "\t%s_0:\n", functionName);
	}

	for (struct LinkedListNode *TACRunner = thisBlock->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
	{
		struct TACLine *thisTAC = TACRunner->data;

		if (thisTAC->operation != tt_asm)
		{
			char *printedTAC = sPrintTACLine(thisTAC);
			fprintf(outFile, "\t;%s\n", printedTAC);
			free(printedTAC);
		}

		switch (thisTAC->operation)
		{
		case tt_asm:
			fputs(thisTAC->operands[0].name.str, outFile);
			fputc('\n', outFile);
			break;

		case tt_assign:
		case tt_cast_assign:
		{
			struct Lifetime *assignedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);

			// assign to literal value
			switch (thisTAC->operands[1].permutation)
			{
			case vp_literal:
			{
				// spilled <- literal
				if (assignedLifetime->isSpilled)
				{
					char *placedRegisterStr = PlaceLiteralInRegister(outFile, thisTAC->operands[1].name.str, reservedRegisters[0]);
					WriteSpilledVariable(outFile, thisScope, assignedLifetime, placedRegisterStr);
				}
				// register <- literal
				else
				{
					PlaceLiteralInRegister(outFile, thisTAC->operands[1].name.str, assignedLifetime->stackOrRegLocation);
				}
			}
			break;

			case vp_standard:
			case vp_temp:
			case vp_objptr:
			{
				char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[0], touchedRegisters);
				// spilled <- ???
				if (assignedLifetime->isSpilled)
				{
					WriteSpilledVariable(outFile, thisScope, assignedLifetime, sourceRegStr);
				}
				// register <- ???
				else
				{
					fprintf(outFile, "\t%s %%r%d, %s\n", SelectMovWidth(thisScope, &thisTAC->operands[0]), assignedLifetime->stackOrRegLocation, sourceRegStr);
				}
			}
			break;

				break;
			}
			break;
		}
		break;

		// place declared localpointer in register if necessary
		// everything else (args and regular variables) handled explicitly elsewhere
		case tt_declare:
		{
			struct Lifetime *declaredLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			if ((declaredLifetime->localPointerTo != NULL) &&
				(!declaredLifetime->isSpilled) &&
				(!declaredLifetime->localPointerTo->isGlobal))
			{
				fprintf(outFile, "\tsubi %%r%d, %%bp, $%d\n", declaredLifetime->stackOrRegLocation, declaredLifetime->localPointerTo->stackOffset * -1);
			}
		}
		break;

		case tt_add:
		case tt_subtract:
		case tt_mul:
		case tt_div:
		{
			struct Lifetime *assignedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			char *destinationRegister;
			if (assignedLifetime->isSpilled)
			{
				destinationRegister = registerNames[reservedRegisters[0]];
			}
			else
			{
				destinationRegister = registerNames[assignedLifetime->stackOrRegLocation];
			}

			if (thisTAC->operands[1].permutation == vp_literal && thisTAC->operands[2].permutation == vp_literal)
			{
				ErrorAndExit(ERROR_INTERNAL, "Arithmetic between two literals!\n");
			}

			// op1 is a literal
			if (thisTAC->operands[1].permutation == vp_literal)
			{
				// op1 and op2 are literals
				if (thisTAC->operands[2].permutation == vp_literal)
				{
					ErrorAndExit(ERROR_INTERNAL, "Arithmetic between two literals!\n");
				}
				// only op1 is a literal
				else
				{
					// try and reorder so we can use an immediate instruction
					if (thisTAC->reorderable)
					{
						char *operand1String = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[2], outFile, reservedRegisters[0], touchedRegisters);
						fprintf(outFile, "\t%si %s, %s, $%s\n", getAsmOp(thisTAC->operation), destinationRegister, operand1String, thisTAC->operands[1].name.str);
					}
					else
					{
						char *operand1String = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[0], touchedRegisters);
						char *operand2String = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[2], outFile, reservedRegisters[1], touchedRegisters);
						fprintf(outFile, "\t%s %s, %s, %s\n", getAsmOp(thisTAC->operation), destinationRegister, operand1String, operand2String);
					}
				}
			}
			else
			{
				// op1 not a literal

				char *operand1String = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[0], touchedRegisters);
				// operand 2 is a literal, use an immediate instruction
				if (thisTAC->operands[2].permutation == vp_literal)
				{
					fprintf(outFile, "\t%si %s, %s, $%s\n", getAsmOp(thisTAC->operation), destinationRegister, operand1String, thisTAC->operands[2].name.str);
				}
				// operand 2 is not a literal
				else
				{
					char *operand2String = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[2], outFile, reservedRegisters[1], touchedRegisters);
					fprintf(outFile, "\t%s %s, %s, %s\n", getAsmOp(thisTAC->operation), destinationRegister, operand1String, operand2String);
				}
			}

			if (assignedLifetime->isSpilled)
			{
				WriteSpilledVariable(outFile, thisScope, assignedLifetime, destinationRegister);
			}
		}
		break;

		case tt_reference:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memw_1:
		{
			char *destRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[0], outFile, reservedRegisters[0], touchedRegisters);
			char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[1], touchedRegisters);
			const char *movOp = SelectMovWidth(thisScope, &thisTAC->operands[0]);
			fprintf(outFile, "\t%s (%s), %s\n", movOp, destRegStr, sourceRegStr);
		}
		break;

		case tt_memw_2:
		case tt_memw_2_n:
		{
			char *baseRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[0], outFile, reservedRegisters[0], touchedRegisters);
			char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[2], outFile, reservedRegisters[1], touchedRegisters);

			const char *movOp = SelectMovWidth(thisScope, &thisTAC->operands[0]);

			if (thisTAC->operation == tt_memw_2)
			{
				fprintf(outFile, "\t%s (%s+%d), %s\n", movOp, baseRegStr, thisTAC->operands[1].name.val, sourceRegStr);
			}
			else
			{
				fprintf(outFile, "\t%s (%s-%d), %s\n", movOp, baseRegStr, thisTAC->operands[1].name.val, sourceRegStr);
			}
		}
		// ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
		break;

		case tt_memw_3:
		case tt_memw_3_n:
		{
			char *baseRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[0], outFile, reservedRegisters[0], touchedRegisters);
			char *offsetRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[1], touchedRegisters);
			char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[3], outFile, reservedRegisters[2], touchedRegisters);
			const char *movOp = SelectMovWidthForDereference(thisScope, &thisTAC->operands[0]);

			if (thisTAC->operation == tt_memw_3)
			{
				fprintf(outFile, "\t%s (%s+%s,%d), %s\n",
						movOp,
						baseRegStr, offsetRegStr,
						thisTAC->operands[2].name.val,
						sourceRegStr);
			}
			else
			{
				fprintf(outFile, "\t%s (%s-%s,%d), %s\n",
						movOp,
						baseRegStr, offsetRegStr,
						thisTAC->operands[2].name.val,
						sourceRegStr);
			}
		}
		break;

		case tt_dereference: // these are redundant... probably makes sense to remove one?
		case tt_memr_1:
		{
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_dereference/tt_memr_w with spilled destination not supported!\n");
			}
			else
			{
				char *destRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[0], outFile, reservedRegisters[0], touchedRegisters);
				char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[1], touchedRegisters);
				const char *movOp = SelectMovWidthForDereference(thisScope, &thisTAC->operands[1]);
				fprintf(outFile, "\t%s %s, (%s)\n", movOp, destRegStr, sourceRegStr);
			}
		}
		break;

		case tt_memr_2:
		case tt_memr_2_n:
		{
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_memr_2 with spilled destination not supported!\n");
			}
			else
			{
				char *destRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[0], outFile, reservedRegisters[0], touchedRegisters);
				char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[1], touchedRegisters);
				const char *movOp = SelectMovWidth(thisScope, &thisTAC->operands[1]);
				if (thisTAC->operation == tt_memr_2)
				{
					fprintf(outFile, "\t%s %s, (%s+%d)\n", movOp, destRegStr, sourceRegStr, thisTAC->operands[2].name.val);
				}
				else
				{
					fprintf(outFile, "\t%s %s, (%s-%d)\n", movOp, destRegStr, sourceRegStr, thisTAC->operands[2].name.val);
				}
			}
		}
		break;

		case tt_memr_3:
		case tt_memr_3_n:
		{
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			char *destRegStr;
			if (destinationLifetime->isSpilled)
			{
				destRegStr = registerNames[reservedRegisters[0]];
			}
			else
			{
				destRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[0], outFile, reservedRegisters[0], touchedRegisters);
			}
			char *baseRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[1], touchedRegisters);
			char *offsetRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[2], outFile, reservedRegisters[2], touchedRegisters);
			const char *movOp = SelectMovWidth(thisScope, &thisTAC->operands[0]);
			if (thisTAC->operation == tt_memr_3)
			{
				fprintf(outFile, "\t%s %s, (%s+%s,%d)\n", movOp, destRegStr, baseRegStr, offsetRegStr, thisTAC->operands[3].name.val);
			}
			else
			{
				fprintf(outFile, "\t%s %s, (%s-%s,%d)\n", movOp, destRegStr, baseRegStr, offsetRegStr, thisTAC->operands[3].name.val);
			}

			if (destinationLifetime->isSpilled)
			{
				WriteSpilledVariable(outFile, thisScope, destinationLifetime, destRegStr);
			}
		}

		break;

		case tt_cmp:
		{
			if (thisTAC->operands[1].permutation == vp_literal && thisTAC->operands[2].permutation == vp_literal)
			{
				ErrorAndExit(ERROR_INTERNAL, "Cmp between two literals!\n");
			}

			// immediate
			if (thisTAC->operands[2].permutation == vp_literal)
			{
				char *op1SourceStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[0], touchedRegisters);

				fprintf(outFile, "\t%si %s, $%s\n", getAsmOp(thisTAC->operation), op1SourceStr, thisTAC->operands[2].name.str);
			}
			else
			{
				char *op1RegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[1], outFile, reservedRegisters[0], touchedRegisters);
				char *op2RegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[2], outFile, reservedRegisters[1], touchedRegisters);

				fprintf(outFile, "\t%s %s, %s\n", getAsmOp(thisTAC->operation), op1RegStr, op2RegStr);
			}
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
			char *opRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[0], outFile, reservedRegisters[0], touchedRegisters);
			fprintf(outFile, "\t%s %s\n", SelectPushWidth(thisScope, &thisTAC->operands[0]), opRegStr);
		}
		break;

		case tt_call:
		{
			fprintf(outFile, "\tcall %s\n", thisTAC->operands[1].name.str);

			// the call returns a value
			if (thisTAC->operands[0].name.str != NULL)
			{
				struct Lifetime *returnedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
				if (returnedLifetime == NULL)
				{
					ErrorAndExit(ERROR_INTERNAL, "Unable to find lifetime for [%s]!\n", thisTAC->operands[0].name.str);
				}

				if (!returnedLifetime->isSpilled)
				{
					fprintf(outFile, "\t%s %%r%d, %%rr\n", SelectMovWidth(thisScope, &thisTAC->operands[0]), returnedLifetime->stackOrRegLocation);
				}
				else
				{
					WriteSpilledVariable(outFile, thisScope, returnedLifetime, registerNames[13]);
				}
			}
		}
		break;

		case tt_label:
			fprintf(outFile, "\t%s_%d:\n", functionName, thisTAC->operands[0].name.val);
			break;

		case tt_return:
		{

			char *destRegStr = placeOrFindOperandInRegister(allLifetimes, thisScope, thisTAC->operands[0], outFile, reservedRegisters[0], touchedRegisters);
			fprintf(outFile, "\tmov %%rr, %s\n", destRegStr);
			fprintf(outFile, "\tjmp %s_done\n", functionName);
		}
		break;

		case tt_do:
		case tt_enddo:
			break;
		}
	}
}
