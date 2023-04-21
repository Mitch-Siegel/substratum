#include "codegen.h"

#define MAX_ASM_LINE_SIZE 256

char printedLine[MAX_ASM_LINE_SIZE];

#define TRIM_APPEND(currentBlock, nChars) (LinkedList_Append(currentBlock, strTrim(printedLine, nChars)))
#define TRIM_PREPEND(currentBlock, nChars) (LinkedList_Prepend(currentBlock, strTrim(printedLine, nChars)))

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
	unsigned int mask = 0b1;
	int i = 0;
	while (mask < size)
	{
		mask <<= 1;
		i++;
	}
	return i;
}

char *PlaceLiteralInRegister(struct LinkedList *currentBlock, char *literalStr, int destReg)
{
	char *destRegStr = registerNames[destReg];
	int literalValue = atoi(literalStr);
	if (literalValue < 0x100)
	{
		TRIM_APPEND(currentBlock, sprintf(printedLine, "movb %s, $%s", destRegStr, literalStr));
	}
	else if (literalValue < 0x10000)
	{
		TRIM_APPEND(currentBlock, sprintf(printedLine, "movh %s, $%s", destRegStr, literalStr));
	}
	else
	{
		int firstHalf, secondHalf;
		firstHalf = literalValue & 0xffff;
		secondHalf = literalValue >> 16;
		char halvedString[16]; // will be long enough to hold any int32/uint32 so can definitely hold half of one

		sprintf(halvedString, "%d", secondHalf);
		TRIM_APPEND(currentBlock, sprintf(printedLine, "movh %s, $%s", destRegStr, halvedString));

		TRIM_APPEND(currentBlock, sprintf(printedLine, "shli %s, $16", destRegStr));

		sprintf(halvedString, "%d", firstHalf);
		TRIM_APPEND(currentBlock, sprintf(printedLine, "movh %s, $%s", destRegStr, halvedString));
	}

	return destRegStr;
}

void WriteSpilledVariable(struct LinkedList *currentBlock, struct Lifetime *writtenTo, char *sourceRegStr)
{
	if (writtenTo->stackOrRegLocation > 0)
	{
		TRIM_APPEND(currentBlock, sprintf(printedLine, "%s (%%bp+%d), %s", SelectMovWidthForLifetime(writtenTo), writtenTo->stackOrRegLocation, sourceRegStr));
	}
	else
	{
		TRIM_APPEND(currentBlock, sprintf(printedLine, "%s (%%bp%d), %s", SelectMovWidthForLifetime(writtenTo), writtenTo->stackOrRegLocation, sourceRegStr));
	}
}

char *ReadSpilledVariable(struct LinkedList *currentBlock, int destReg, struct Lifetime *readFrom)
{
	char *destRegStr = registerNames[destReg];

	if (readFrom->stackOrRegLocation > 0)
	{
		TRIM_APPEND(currentBlock, sprintf(printedLine, "%s %s, (%%bp+%d)", SelectMovWidthForLifetime(readFrom), destRegStr, readFrom->stackOrRegLocation));
	}
	else
	{
		TRIM_APPEND(currentBlock, sprintf(printedLine, "%s %s, (%%bp%d)", SelectMovWidthForLifetime(readFrom), destRegStr, readFrom->stackOrRegLocation));
	}
	return destRegStr;
}

// places an operand by name into the specified register, or returns the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
char *placeOrFindOperandInRegister(struct LinkedList *lifetimes, struct TACOperand operand, struct LinkedList *currentBlock, int registerIndex, char *touchedRegisters)
{

	if (operand.permutation == vp_literal)
	{
		if (registerIndex < 0)
		{
			ErrorAndExit(ERROR_INTERNAL, "Expected scratch register to place literal in, didn't get one!");
		}

		return PlaceLiteralInRegister(currentBlock, operand.name.str, registerIndex);
	}

	struct Lifetime *relevantLifetime = LinkedList_Find(lifetimes, compareLifetimes, operand.name.str);
	if (relevantLifetime == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to find lifetime for variable %s!\n", operand.name.str);
	}

	if (registerIndex < 0 && relevantLifetime->isSpilled)
	{
		ErrorAndExit(ERROR_INTERNAL, "Call to attempt to place spilled variable %s when none should be spilled!", operand.name.str);
	}
	// else the variable is in a register already

	// if not a local pointer, the value for this variable *must* exist either in a register or spilled on the stack
	if (relevantLifetime->localPointerTo == NULL)
	{
		if (relevantLifetime->isSpilled)
		{
			char *resultRegisterStr = ReadSpilledVariable(currentBlock, registerIndex, relevantLifetime);
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
			char *constructLocalPointerLine = malloc(64);
			int basepointerOffset = relevantLifetime->localPointerTo->stackOffset;
			if (basepointerOffset > 0)
			{
				sprintf(constructLocalPointerLine, "addi %s, %%bp, $%d", destRegStr, basepointerOffset);
			}
			else if (basepointerOffset < 0)
			{
				sprintf(constructLocalPointerLine, "subi %s, %%bp, $%d", destRegStr, -1 * basepointerOffset);
			}
			else
			{
				sprintf(constructLocalPointerLine, "mov %s, %%bp", destRegStr);
			}
			LinkedList_Append(currentBlock, constructLocalPointerLine);
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

struct Stack *generateCode(struct SymbolTable *table, FILE *outFile)
{
	struct Stack *scopeBlocks = Stack_New();
	for (int i = 0; i < table->globalScope->entries->size; i++)
	{
		struct ScopeMember *thisMember = table->globalScope->entries->data[i];
		switch (thisMember->type)
		{
		case e_function:
			Stack_Push(scopeBlocks, generateCodeForFunction(thisMember->entry, outFile));
			break;

		case e_basicblock:
		{
			struct LinkedList *blockBlock = LinkedList_New();
			char touchedRegisters[MACHINE_REGISTER_COUNT];
			for (int i = 0; i < MACHINE_REGISTER_COUNT; i++)
			{
				touchedRegisters[i] = 0;
			}
			int reservedRegisters[3];

			GenerateCodeForBasicBlock(thisMember->entry, table->globalScope, NULL, blockBlock, "global", reservedRegisters, touchedRegisters);
			Stack_Push(scopeBlocks, blockBlock);
		}
		break;

		default:
			break;
		}
	}
	return scopeBlocks;
};

/*
 * code generation for funcitons (lifetime management, etc)
 *
 */
struct LinkedList *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile)
{
	printf("generate code for function %s", function->name);

	struct CodegenMetadata metadata;
	metadata.function = function;
	metadata.allLifetimes = findLifetimes(function);

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
		int thisSize = GetSizeOfPrimitive(thisLifetime->type);

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

	/*
	for (struct LinkedListNode *ltRunner = metadata.allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
	{
		struct Lifetime *thisLifetime = ltRunner->data;
		if (thisLifetime->isSpilled)
		{
			printf("%%bp + %-3d", thisLifetime->stackOrRegLocation);
		}
		else
		{
			printf("%%r%d      ", thisLifetime->stackOrRegLocation);
		}
		printf(": %-16s ", thisLifetime->name);
		if (thisLifetime->indirectionLevel)
		{
			printf("(size 4) ");
		}
		else
		{
			printf("(size %d) ", GetSizeOfPrimitive(thisLifetime->type));
		}

		printf("localPointer? %d\n", thisLifetime->localPointerTo != NULL);
	}*/

	// actual registers have been assigned to variables
	printf(".");

	/*
	printf("\n%17s", "");
	for (int i = 0; i < metadata.largestTacIndex; i++)
	{
		printf("%x", i >> 4);
	}
	printf("\n");
	printf("%17s", "");
	for (int i = 0; i < metadata.largestTacIndex; i++)
	{
		printf("%x", i & 0xf);
	}
	printf("\n");

	for (struct LinkedListNode *ltRunner = metadata.allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
	{
		struct Lifetime *thisLifetime = ltRunner->data;
		printf("%16s:", thisLifetime->name);
		for (int i = 0; i <= metadata.largestTacIndex; i++)
		{
			if (i >= thisLifetime->start && i <= thisLifetime->end)
			{
				if (thisLifetime->isSpilled)
				{
					printf("S");
				}
				else
				{
					printf("#");
				}
			}
			else
			{
				printf(" ");
			}
		}
		printf("\n");
		// printf("%s\t:Spilled:%d Location:%d Lifetime:%2d-%2d\n", thisLifetime->variable, thisLifetime->isSpilled, thisLifetime->stackOrRegLocation, thisLifetime->start, thisLifetime->end);
	}
		*/

	struct LinkedList *functionBlock = LinkedList_New();

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
				const char *movOp = SelectMovWidthForLifetime(thisLifetime);
				TRIM_APPEND(functionBlock, sprintf(printedLine, "%s %%r%d, (%%bp+%d) ;place %s", movOp, thisLifetime->stackOrRegLocation, theArgument->stackOffset, thisLifetime->name));
				metadata.touchedRegisters[thisLifetime->stackOrRegLocation] = 1;
			}
		}
	}

	// arguments placed into registers
	printf(".");

	for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
	{
		struct BasicBlock *thisBlock = blockRunner->data;
		GenerateCodeForBasicBlock(thisBlock, function->mainScope, metadata.allLifetimes, functionBlock, function->name, metadata.reservedRegisters, metadata.touchedRegisters);
	}

	// meaningful code generated
	printf(".");
	// TRIM_APPEND(currentBlock, sprintf(printedLine

	TRIM_APPEND(functionBlock, sprintf(printedLine, "%s_done:", function->name));

	for (int i = REGISTERS_TO_ALLOCATE - 1; i >= 0; i--)
	{
		if (metadata.touchedRegisters[i])
		{
			TRIM_APPEND(functionBlock, sprintf(printedLine, "pop %%r%d", i));

			TRIM_PREPEND(functionBlock, sprintf(printedLine, "push %%r%d", i));
		}
	}

	if (stackOffset > 0)
	{
		TRIM_PREPEND(functionBlock, sprintf(printedLine, "subi %%sp, %%sp, $%d", stackOffset));
	}

	TRIM_PREPEND(functionBlock, sprintf(printedLine, "%s:", function->name));

	TRIM_PREPEND(functionBlock, sprintf(printedLine, "#align 2048"));

	if (stackOffset > 0)
	{
		TRIM_APPEND(functionBlock, sprintf(printedLine, "addi %%sp, %%sp, $%d", stackOffset));
	}

	if (function->argStackSize > 0)
	{
		TRIM_APPEND(functionBlock, sprintf(printedLine, "ret %d", function->argStackSize));
	}
	else
	{
		TRIM_APPEND(functionBlock, sprintf(printedLine, "ret"));
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
	return functionBlock;
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

const char *SelectMovWidthForPrimitive(enum variableTypes type)
{
	return SelectMovWidthForSize(GetSizeOfPrimitive(type));
}

const char *SelectMovWidth(struct TACOperand *dataDest)
{
	// pointers are always full-width
	if (dataDest->indirectionLevel > 0)
	{
		return "mov";
	}

	return SelectMovWidthForSize(GetSizeOfPrimitive(dataDest->type));
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

const char *SelectMovWidthForLifetime(struct Lifetime *lifetime)
{
	if (lifetime->indirectionLevel > 0)
	{
		return "mov";
	}
	else
	{
		return SelectMovWidthForPrimitive(lifetime->type);
	}
}

const char *SelectPushWidthForPrimitive(enum variableTypes type)
{
	return SelectPushWidthForSize(GetSizeOfPrimitive(type));
}

const char *SelectPushWidth(struct TACOperand *dataDest)
{
	// pointers are always full-width
	if (dataDest->indirectionLevel > 0)
	{
		return "push";
	}

	return SelectPushWidthForSize(GetSizeOfPrimitive(dataDest->type));
}

// TODO: thisBlock vs asmBlock?!
void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock,
							   struct Scope *thisScope,
							   struct LinkedList *allLifetimes,
							   struct LinkedList *asmBlock,
							   char *functionName,
							   int reservedRegisters[3],
							   char *touchedRegisters)
{
	// TODO: generate localpointers as necessary
	// to registers for those that get them and on-the-fly for those that don't
	if (thisBlock->labelNum > 0)
	{
		TRIM_APPEND(asmBlock, sprintf(printedLine, "%s_%d:", functionName, thisBlock->labelNum));
	}
	else
	{
		TRIM_APPEND(asmBlock, sprintf(printedLine, "%s_0:", functionName));
	}

	for (struct LinkedListNode *TACRunner = thisBlock->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
	{
		struct TACLine *thisTAC = TACRunner->data;

		if (thisTAC->operation != tt_asm)
		{
			char *printedTAC = sPrintTACLine(thisTAC);
			TRIM_APPEND(asmBlock, sprintf(printedLine, ";%s", printedTAC));
			free(printedTAC);
		}

		switch (thisTAC->operation)
		{
		case tt_asm:
			LinkedList_Append(asmBlock, thisTAC->operands[0].name.str);
			break;

		case tt_assign:
		case tt_cast_assign:
		{
			struct Lifetime *assignedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);

			// assign to literal value
			if (thisTAC->operands[1].permutation == vp_literal)
			{
				// spilled <- literal
				if (assignedLifetime->isSpilled)
				{
					char *placedRegisterStr = PlaceLiteralInRegister(asmBlock, thisTAC->operands[1].name.str, reservedRegisters[0]);
					WriteSpilledVariable(asmBlock, assignedLifetime, placedRegisterStr);
				}
				// register <- literal
				else
				{
					PlaceLiteralInRegister(asmBlock, thisTAC->operands[1].name.str, assignedLifetime->stackOrRegLocation);
				}
			}
			else
			{
				char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[0], touchedRegisters);
				// spilled <- ???
				if (assignedLifetime->isSpilled)
				{
					WriteSpilledVariable(asmBlock, assignedLifetime, sourceRegStr);
				}
				// register <- ???
				else
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, %s", SelectMovWidth(&thisTAC->operands[0]), assignedLifetime->stackOrRegLocation, sourceRegStr));
				}
			}
		}
		break;

		// place declared localpointer in register if necessary
		// everything else (args and regular variables) handled explicitly elsewhere
		case tt_declare:
		{
			struct Lifetime *declaredLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			if ((declaredLifetime->localPointerTo != NULL) &&
				(!declaredLifetime->isSpilled))
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "subi %%r%d, %%bp, $%d", declaredLifetime->stackOrRegLocation, declaredLifetime->localPointerTo->stackOffset * -1));
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
						char *operand1String = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, reservedRegisters[0], touchedRegisters);
						TRIM_APPEND(asmBlock, sprintf(printedLine, "%si %s, %s, $%s", getAsmOp(thisTAC->operation), destinationRegister, operand1String, thisTAC->operands[1].name.str));
					}
					else
					{
						char *operand1String = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[0], touchedRegisters);
						char *operand2String = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, reservedRegisters[1], touchedRegisters);
						TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, operand1String, operand2String));
					}
				}
			}
			else
			{
				// op1 not a literal

				char *operand1String = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[0], touchedRegisters);
				// operand 2 is a literal, use an immediate instruction
				if (thisTAC->operands[2].permutation == vp_literal)
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%si %s, %s, $%s", getAsmOp(thisTAC->operation), destinationRegister, operand1String, thisTAC->operands[2].name.str));
				}
				// operand 2 is not a literal
				else
				{
					char *operand2String = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, reservedRegisters[1], touchedRegisters);
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, operand1String, operand2String));
				}
			}

			if (assignedLifetime->isSpilled)
			{
				WriteSpilledVariable(asmBlock, assignedLifetime, destinationRegister);
			}
		}
		break;

		case tt_reference:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memw_1:
		{
			char *destRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%s), %s", movOp, destRegStr, sourceRegStr));
		}
		break;

		case tt_memw_2:
		case tt_memw_2_n:
		{
			char *baseRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, reservedRegisters[1], touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
			if (thisTAC->operation == tt_memw_2)
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%s+%d), %s", movOp, baseRegStr, thisTAC->operands[1].name.val, sourceRegStr));
			}
			else
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%s-%d), %s", movOp, baseRegStr, thisTAC->operands[1].name.val, sourceRegStr));
			}
		}
		// ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
		break;

		case tt_memw_3:
		case tt_memw_3_n:
		{
			char *baseRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			char *offsetRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
			char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[3], asmBlock, reservedRegisters[2], touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);

			if (thisTAC->operation == tt_memw_3)
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%s+%s,%d), %s", movOp, baseRegStr, offsetRegStr, ALIGNSIZE(GetSizeOfPrimitive(thisTAC->operands[0].type)), sourceRegStr));
			}
			else
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%s-%s,%d), %s", movOp, baseRegStr, offsetRegStr, ALIGNSIZE(GetSizeOfPrimitive(thisTAC->operands[0].type)), sourceRegStr));
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
				char *destRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
				char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
				const char *movOp = SelectMovWidth(&thisTAC->operands[1]);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, (%s)", movOp, destRegStr, sourceRegStr));
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
				char *destRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
				char *sourceRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
				const char *movOp = SelectMovWidth(&thisTAC->operands[1]);
				if (thisTAC->operation == tt_memr_2)
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, (%s+%d)", movOp, destRegStr, sourceRegStr, thisTAC->operands[2].name.val));
				}
				else
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, (%s-%d)", movOp, destRegStr, sourceRegStr, thisTAC->operands[2].name.val));
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
				destRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			}
			char *baseRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
			char *offsetRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, reservedRegisters[2], touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
			if (thisTAC->operation == tt_memr_3)
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, (%s+%s,%d)", movOp, destRegStr, baseRegStr, offsetRegStr, ALIGNSIZE(GetSizeOfPrimitive(thisTAC->operands[0].type))));
			}
			else
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, (%s-%s,%d)", movOp, destRegStr, baseRegStr, offsetRegStr, ALIGNSIZE(GetSizeOfPrimitive(thisTAC->operands[0].type))));
			}

			if (destinationLifetime->isSpilled)
			{
				WriteSpilledVariable(asmBlock, destinationLifetime, destRegStr);
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
				char *op1SourceStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[0], touchedRegisters);

				TRIM_APPEND(asmBlock, sprintf(printedLine, "%si %s, $%s", getAsmOp(thisTAC->operation), op1SourceStr, thisTAC->operands[2].name.str));
			}
			else
			{
				char *op1RegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[0], touchedRegisters);
				char *op2RegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, reservedRegisters[1], touchedRegisters);

				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, %s", getAsmOp(thisTAC->operation), op1RegStr, op2RegStr));
			}
		}
		break;

		case tt_jg:
		case tt_jge:
		case tt_jl:
		case tt_jle:
		case tt_je:
		case tt_jne:
		{
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s_%d", getAsmOp(thisTAC->operation), functionName, thisTAC->operands[0].name.val));
		}
		break;

		case tt_jmp:
		{
			TRIM_APPEND(asmBlock, sprintf(printedLine, "jmp %s_%d", functionName, thisTAC->operands[0].name.val));
		}
		break;

		case tt_push:
		{
			char *opRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s", SelectPushWidth(&thisTAC->operands[0]), opRegStr));
		}
		break;

		case tt_call:
		{
			TRIM_APPEND(asmBlock, sprintf(printedLine, "call %s", thisTAC->operands[1].name.str));

			// the call returns a value
			if (thisTAC->operands[0].name.str != NULL)
			{
				struct Lifetime *returnedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
				if (!returnedLifetime->isSpilled)
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, %%rr", SelectMovWidth(&thisTAC->operands[0]), returnedLifetime->stackOrRegLocation));
				}
				else
				{
					WriteSpilledVariable(asmBlock, returnedLifetime, registerNames[13]);
				}
			}
		}
		break;

		case tt_label:
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s_%d:", functionName, thisTAC->operands[0].name.val));
			break;

		case tt_return:
		{

			char *destRegStr = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			TRIM_APPEND(asmBlock, sprintf(printedLine, "mov %%rr, %s", destRegStr));
			TRIM_APPEND(asmBlock, sprintf(printedLine, "jmp %s_done", functionName));
		}
		break;

		case tt_do:
		case tt_enddo:
			break;
		}
	}
}
