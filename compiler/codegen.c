#include "codegen.h"

#define MAX_ASM_LINE_SIZE 256

char printedLine[MAX_ASM_LINE_SIZE];

#define TRIM_APPEND(currentBlock, nChars) (LinkedList_Append(currentBlock, strTrim(printedLine, nChars)))
#define TRIM_PREPEND(currentBlock, nChars) (LinkedList_Prepend(currentBlock, strTrim(printedLine, nChars)))

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

void PlaceLiteralInRegister(struct LinkedList *currentBlock, char *literalStr, char *destReg)
{
	int literalValue = atoi(literalStr);
	if (literalValue < 0x100)
	{
		TRIM_APPEND(currentBlock, sprintf(printedLine, "movb %s, $%s", destReg, literalStr));
	}
	else if (literalValue < 0x10000)
	{
		TRIM_APPEND(currentBlock, sprintf(printedLine, "movh %s, $%s", destReg, literalStr));
	}
	else
	{
		int firstHalf, secondHalf;
		firstHalf = literalValue & 0xffff;
		secondHalf = literalValue >> 16;
		char halvedString[16]; // will be long enough to hold any int32/uint32 so can definitely hold half of one

		sprintf(halvedString, "%d", secondHalf);
		TRIM_APPEND(currentBlock, sprintf(printedLine, "movh %s, $%s", destReg, halvedString));

		TRIM_APPEND(currentBlock, sprintf(printedLine, "shli %s, $16", destReg));

		sprintf(halvedString, "%d", firstHalf);
		TRIM_APPEND(currentBlock, sprintf(printedLine, "movh %s, $%s", destReg, halvedString));
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
			char touchedRegisters[REGISTER_COUNT];
			int reservedRegisters[2];
			reservedRegisters[0] = 0;
			reservedRegisters[1] = 1;
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

int calculateRegisterLoading(struct LinkedList *activeLifetimes, int index)
{
	int trueLoad = 0;
	for (struct LinkedListNode *runner = activeLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		if (thisLifetime->start <= index && index < thisLifetime->end)
		{
			trueLoad++;
		}
		else if (thisLifetime->end >= index)
		{
			trueLoad--;
		}
	}
	return trueLoad;
}

struct FunctionRegisterAllocationMetadata
{
	struct FunctionEntry *function; // symbol table entry for the function the register allocation data is for

	struct LinkedList *allLifetimes; // every lifetime that exists within this function

	// array allocated (of size largestTacIndex) for liveness analysis
	// index i contains a linkedList of all lifetimes active at TAC index i
	struct LinkedList **lifetimeOverlaps;

	// tracking for specialized lifetimes which may be removed from lifetimeOverlaps and need to be explicitly tracked
	struct Stack *spilledLifetimes;
	struct Stack *localPointerLifetimes;

	// largest TAC index for any basic block within the function
	int largestTacIndex;

	// flag 2 registers which should be used as scratch in case we have spilled variables (not always used)
	int reservedRegisters[2];

	// flag registers which have *ever* been used so we know what to callee-save
	char touchedRegisters[REGISTER_COUNT];
};

// populate a linkedlist array so that the list at index i contains all lifetimes active at TAC index i
// then determine which variables should be spilled
int generateLifetimeOverlaps(struct FunctionRegisterAllocationMetadata *metadata)
{
	int mostConcurrentLifetimes = 0;

	// populate the array of active lifetimes
	for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;

		// if this lifetime must be spilled (has address-of operator used) (for future use - nothing currently uses(?)), add directly to the spilled list
		struct ScopeMember *thisVariableEntry = Scope_lookup(metadata->function->mainScope, thisLifetime->variable);

		// if we have an argument, make sure to note that it is active at index 0
		// this ensures that arguments that aren't used in code are still tracked (applies particularly to asm-only functions)
		if (thisLifetime->isArgument)
		{
			LinkedList_Append(metadata->lifetimeOverlaps[0], thisLifetime);
		}

		if (thisVariableEntry != NULL && ((struct VariableEntry *)thisVariableEntry->entry)->mustSpill)
		{
			thisLifetime->isSpilled = 1;
			Stack_Push(metadata->spilledLifetimes, thisLifetime);
		}
		// otherwise, put this lifetime into contention for a register
		else
		{
			// if we have a local pointer, make sure we track it in the localpointers stack
			// but also put it into contention for a register, we will just prefer to spill localpointers first
			if (thisVariableEntry != NULL && ((struct VariableEntry *)thisVariableEntry->entry)->localPointerTo != NULL)
			{
				thisLifetime->localPointerTo = ((struct VariableEntry *)thisVariableEntry->entry)->localPointerTo;
				thisLifetime->stackOrRegLocation = -1;
				Stack_Push(metadata->localPointerLifetimes, thisLifetime);
			}
			for (int i = thisLifetime->start; i <= thisLifetime->end; i++)
			{
				LinkedList_Append(metadata->lifetimeOverlaps[i], thisLifetime);
				if (metadata->lifetimeOverlaps[i]->size > mostConcurrentLifetimes)
				{
					mostConcurrentLifetimes = metadata->lifetimeOverlaps[i]->size;
				}
			}
		}
	}
	// printf("Function %s has at most %d concurrent lifetimes (largest TAC index %d)\n", metadata->function->name, mostConcurrentLifetimes, metadata->largestTacIndex);
	return mostConcurrentLifetimes;
}

// return the heuristic for how good a given lifetime is to spill - higher is better
int lifetimeHeuristic(struct Lifetime *lt)
{
	// base heuristic is lifetime length
	int h = lt->end - lt->start;
	// add the number of reads for this variable since they have some cost
	h += lt->nreads;
	// multiply by number of writes for this variable since that is a high-cost operation
	h *= lt->nwrites;

	// inflate heuristics for cases which have no actual stack space cost to spill:
	// super-prefer to "spill" arguments as they already have a stack address
	if (lt->isArgument)
	{
		h *= 1000;
	}

	// secondarily prefer to "spill" pointers to local objects
	// they can be generated on-the-fly from the base pointer with 1 arithmetic instruction
	else if (lt->localPointerTo != NULL)
	{
		h *= 100;
	}

	return h;
}

void spillVariables(struct FunctionRegisterAllocationMetadata *metadata, int mostConcurrentLifetimes)
{
	int MAXREG = REGISTER_COUNT;
	// if we have just enough room, simply use all registers
	// if we need to spill, ensure 2 scratch registers
	if (mostConcurrentLifetimes >= REGISTER_COUNT)
	{
		MAXREG -= 2;
		metadata->reservedRegisters[0] = 0;
		metadata->reservedRegisters[1] = 1;
	}
	else
	{
		metadata->reservedRegisters[0] = mostConcurrentLifetimes;
		metadata->reservedRegisters[1] = mostConcurrentLifetimes + 1;
	}

	// look through the populated array of active lifetimes
	// if a given index has too many active lifetimes, figure out which lifetime(s) to spill
	// then allocate registers for any lifetimes without a home
	for (int i = 0; i <= metadata->largestTacIndex; i++)
	{
		while (calculateRegisterLoading(metadata->lifetimeOverlaps[i], i) > MAXREG)
		{
			struct LinkedListNode *overlapRunner = metadata->lifetimeOverlaps[i]->head;

			// start off the best heuristic as the first item
			struct Lifetime *bestLifetime = (struct Lifetime *)overlapRunner->data;
			int bestHeuristic = lifetimeHeuristic(bestLifetime);

			for (; overlapRunner != NULL; overlapRunner = overlapRunner->next)
			{
				struct Lifetime *thisLifetime = overlapRunner->data;

				int thisHeuristic = lifetimeHeuristic(thisLifetime);

				// printf("%s has heuristic of %f\n", thisLifetime->variable, thisHeuristic);
				if (thisHeuristic > bestHeuristic)
				{
					bestHeuristic = thisHeuristic;
					bestLifetime = thisLifetime;
				}
			}

			// this method actually deletes the spilled variable from the liveness array
			// if it becomes necessary to keep the untouched liveness array around, it will need to be copied within this function
			for (int j = bestLifetime->start; j <= bestLifetime->end; j++)
			{
				LinkedList_Delete(metadata->lifetimeOverlaps[j], compareLifetimes, bestLifetime->variable);
			}
			bestLifetime->isSpilled = 1;
			Stack_Push(metadata->spilledLifetimes, bestLifetime);
		}
	}
}

// sort the list of spilled lifetimes by size of the variable so they can be laid out cleanly on the stack
void sortSpilledLifetimes(struct FunctionRegisterAllocationMetadata *metadata)
{
	// TODO: handle arrays properly?
	//  - this shouldn't be a consideration because the only lifetimes that are considered to be spilled are localpointers?
	//  - but localpointers will still be in the spilled list if they aren't being kept in registers, because they need somewhere to be
	//     despite the fact that they really don't actually be spilled to stack, instead just generated from base pointer and offset

	// simple bubble sort
	for (int i = 0; i < metadata->spilledLifetimes->size; i++)
	{
		for (int j = 0; j < metadata->spilledLifetimes->size - i - 1; j++)
		{
			struct Lifetime *thisLifetime = metadata->spilledLifetimes->data[j];
			int thisSize = Scope_getSizeOfVariableByString(metadata->function->mainScope, thisLifetime->variable);
			int compSize = Scope_getSizeOfVariableByString(metadata->function->mainScope, ((struct Lifetime *)metadata->spilledLifetimes->data[j + 1])->variable);

			if (thisSize < compSize)
			{
				struct Lifetime *swap = metadata->spilledLifetimes->data[j];
				metadata->spilledLifetimes->data[j] = metadata->spilledLifetimes->data[j + 1];
				metadata->spilledLifetimes->data[j + 1] = swap;
			}
		}
	}
}

void assignRegisters(struct FunctionRegisterAllocationMetadata *metadata)
{
	// printf("\nassigning registers\n");
	// flag registers in use at any given TAC index so we can easily assign
	char registers[REGISTER_COUNT];
	struct Lifetime *occupiedBy[REGISTER_COUNT];

	for (int i = 0; i < REGISTER_COUNT; i++)
	{
		registers[i] = 0;
		occupiedBy[i] = NULL;
		metadata->touchedRegisters[i] = 0;
	}

	// reserve scratch registers for arithmetic
	registers[metadata->reservedRegisters[0]] = 1;
	registers[metadata->reservedRegisters[1]] = 1;

	for (int i = 0; i <= metadata->largestTacIndex; i++)
	{
		// free any registers inhabited by expired lifetimes
		for (int j = 0; j < REGISTER_COUNT; j++)
		{
			if (occupiedBy[j] != NULL && occupiedBy[j]->end <= i)
			{
				// printf("%s expires at %d\n", occupiedBy[j]->variable, i);
				registers[j] = 0;
				occupiedBy[j] = NULL;
			}
		}

		// iterate all lifetimes and assign newly-live ones to a register
		for (struct LinkedListNode *ltRunner = metadata->allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
		{
			struct Lifetime *thisLifetime = ltRunner->data;
			if ((thisLifetime->start == i) &&			  // if the lifetime starts at this step
				(thisLifetime->isSpilled == 0) &&		  // lifetime expects to be in a register
				(thisLifetime->stackOrRegLocation == -1)) // lifetime does not already have a register somehow (redundancy check)
			{
				char registerFound = 0;
				// scan through all registers, looking for an unoccupied one
				for (int j = 0; j < REGISTER_COUNT; j++)
				{
					if (registers[j] == 0)
					{
						// printf("\tAssign register %d for variable %s\n", j, thisLifetime->variable);
						thisLifetime->stackOrRegLocation = j;
						registers[j] = 1;
						occupiedBy[j] = thisLifetime;
						metadata->touchedRegisters[j] = 1;
						registerFound = 1;
						break;
					}
				}
				// no unoccupied register found (redundancy check)
				if (!registerFound)
				{
					/*
					 * if we hit this, either:
					 * 1: something messed up in this function and we ended up with no register to assign this lifetime to
					 * 2: something messed up before we got to this function and too many concurrent lifetimes have been allowed to expect a register assignment
					 */

					ErrorAndExit(ERROR_INTERNAL, "Unable to find register for variable %s!\n", thisLifetime->variable);
				}
			}
		}
	}
}

struct LinkedList *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile)
{
	printf("generate code for function %s", function->name);

	struct FunctionRegisterAllocationMetadata metadata;
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

	metadata.reservedRegisters[0] = -1;
	metadata.reservedRegisters[1] = -1;

	int mostConcurrentLifetimes = generateLifetimeOverlaps(&metadata);

	// if there are too many concurrent lifetimes to all fit in registers, spill some of them
	if (mostConcurrentLifetimes > REGISTER_COUNT)
	{
		spillVariables(&metadata, mostConcurrentLifetimes);
	}

	sortSpilledLifetimes(&metadata);

	// find the total size of the function's stack frame containing local variables *and* spilled variables
	int stackOffset = function->localStackSize; // start with just the things guaranteed to be on the local stack
	for (int i = 0; i < metadata.spilledLifetimes->size; i++)
	{
		struct Lifetime *thisLifetime = metadata.spilledLifetimes->data[i];
		int thisSize = Scope_getSizeOfVariableByString(function->mainScope, thisLifetime->variable);
		struct ScopeMember *thisVariableEntry = Scope_lookup(function->mainScope, thisLifetime->variable);
		if (thisVariableEntry != NULL)
		{
			struct VariableEntry *thisVariable = thisVariableEntry->entry;
			if (thisVariableEntry->type == e_argument)
			{
				thisLifetime->stackOrRegLocation = thisVariable->stackOffset;
			}
			else
			{
				// constant offset of -2 for return address
				thisLifetime->stackOrRegLocation = (-1 * stackOffset) - 2;
				stackOffset += thisSize;
			}
		}
	}

	assignRegisters(&metadata);

	// actual registers have been assigned to variables
	printf(".");
	// move any applicable arguments into registers if we are expecting them not to be spilled
	for (struct LinkedListNode *ltRunner = metadata.allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
	{
		struct Lifetime *thisLifetime = ltRunner->data;
		printf("%s\t:Spilled:%d Location:%d Lifetime:%2d-%2d\n", thisLifetime->variable, thisLifetime->isSpilled, thisLifetime->stackOrRegLocation, thisLifetime->start, thisLifetime->end);
	}

	struct LinkedList *functionBlock = LinkedList_New();

	// move any applicable arguments into registers if we are expecting them not to be spilled
	for (struct LinkedListNode *ltRunner = metadata.allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
	{
		struct Lifetime *thisLifetime = ltRunner->data;

		// (short-circuit away from looking up temps since they can't be arguments)
		if (thisLifetime->variable[0] != '.')
		{
			struct ScopeMember *thisEntry = Scope_lookup(function->mainScope, thisLifetime->variable);
			// we need to place this variable into its register if:
			if ((thisEntry != NULL) &&							 // it exists
				(thisEntry->type == e_argument) &&				 // it's an argument
				(!thisLifetime->isSpilled) &&					 // they're not spilled
				(thisLifetime->nreads || thisLifetime->nwrites)) // theyre are either read from or written to at all
			{
				struct VariableEntry *theArgument = thisEntry->entry;
				const char *movOp = SelectMovWidthForPrimitive(thisLifetime->type);
				TRIM_APPEND(functionBlock, sprintf(printedLine, "%s %%r%d, (%%bp+%d) ;place %s", movOp, thisLifetime->stackOrRegLocation, theArgument->stackOffset, thisLifetime->variable));
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

	for (int i = REGISTER_COUNT - 1; i >= 0; i--)
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
							   int reservedRegisters[2],
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
			struct Lifetime *assignerLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[1].name.str);
			// assign to register
			const char *movInstruction = SelectMovWidth(&thisTAC->operands[0]);

			if (assignedLifetime->isSpilled == 0)
			{
				switch (thisTAC->operands[1].permutation)
				{
				case vp_standard:
				case vp_temp:
					if (!assignerLifetime->isSpilled)
					{
						TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, %%r%d", movInstruction, assignedLifetime->stackOrRegLocation, assignerLifetime->stackOrRegLocation));
					}
					else
					{
						TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, (%%bp+%d*1)", movInstruction, assignedLifetime->stackOrRegLocation, assignerLifetime->stackOrRegLocation));
					}
					break;

				case vp_literal:
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, $%s", movInstruction, assignedLifetime->stackOrRegLocation, thisTAC->operands[1].name.str));
					break;
				}
			}
			// assign to spilled
			else
			{
				switch (thisTAC->operands[1].permutation)
				{
				case vp_standard:
				case vp_temp:
					if (!assignerLifetime->isSpilled)
					{
						TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%%bp+%d), %%r%d", movInstruction, assignedLifetime->stackOrRegLocation, assignerLifetime->stackOrRegLocation));
					}
					else
					{
						// in this case, the value being read needs to be pulled down to a register
						// then put back up onto the stack to assign to the spilled variable
						ErrorAndExit(ERROR_INTERNAL, "assign from spilled to spilled!\n");
					}
					break;

				case vp_literal:
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%%bp+%d), $%s", movInstruction, assignedLifetime->stackOrRegLocation, thisTAC->operands[1].name.str));
				}
				break;
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
			char immediateInstruction = 0;

			struct Lifetime *assignedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			struct Lifetime *operand1Lifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[1].name.str);
			struct Lifetime *operand2Lifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[2].name.str);
			int destinationRegister;
			char op1[12];
			char op2[12];
			char reordering = 0;
			if (assignedLifetime->isSpilled)
			{
				destinationRegister = 0;
			}
			else
			{
				destinationRegister = assignedLifetime->stackOrRegLocation;
			}

			if (thisTAC->operands[1].permutation == vp_literal && thisTAC->operands[2].permutation == vp_literal)
			{
				ErrorAndExit(ERROR_INTERNAL, "Arithmetic between two literals!\n");
			}

			// examine the first operand, place it in regisetr if necessary, and handle reordering (if possible/beneficial)
			switch (thisTAC->operands[1].permutation)
			{
			case vp_standard:
			case vp_temp:
				if (!operand1Lifetime->isSpilled)
				{
					sprintf(op1, "%%r%d", operand1Lifetime->stackOrRegLocation);
				}
				else
				{
					placeOrFindOperandInRegister(allLifetimes, operand1Lifetime->variable, asmBlock, reservedRegisters[0], touchedRegisters);
					sprintf(op1, "%%r%d", reservedRegisters[0]);
				}
				break;

			case vp_literal:
				if (thisTAC->reorderable)
				{
					reordering = 1;
					immediateInstruction = 1;
					sprintf(op1, "$%s", thisTAC->operands[1].name.str);
				}
				break;
			}

			// examine the second operand, place it in register
			switch (thisTAC->operands[2].permutation)
			{
			case vp_standard:
			case vp_temp:
				if (!operand2Lifetime->isSpilled)
				{
					sprintf(op2, "%%r%d", operand2Lifetime->stackOrRegLocation);
				}
				else
				{
					placeOrFindOperandInRegister(allLifetimes, operand2Lifetime->variable, asmBlock, reservedRegisters[1], touchedRegisters);
					sprintf(op2, "%%r%d", reservedRegisters[1]);
				}
				break;

			case vp_literal:
				sprintf(op2, "$%s", thisTAC->operands[2].name.str);
				immediateInstruction = 1;
				break;
			}

			if (reordering)
			{
				if (immediateInstruction)
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%si %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op2, op1));
				}
				else
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op2, op1));
				}
			}
			else
			{
				if (immediateInstruction)
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%si %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op1, op2));
				}
				else
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op1, op2));
				}
			}

			if (assignedLifetime->isSpilled)
			{
				const char *movOp = SelectMovWidthForPrimitive(assignedLifetime->type);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%%bp+%d), %%r%d;replace %s", movOp, assignedLifetime->stackOrRegLocation, destinationRegister, assignedLifetime->variable));
			}
		}
		break;

		case tt_reference:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memw_1:
		{
			int dstIndexReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
			int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%%r%d), %%r%d", movOp, dstIndexReg, sourceReg));
		}
		break;

		case tt_memw_2:
		{
			int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
			int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%%r%d+%d), %%r%d", movOp, baseReg, thisTAC->operands[1].name.val, sourceReg));
		}
		// ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
		break;

		case tt_memw_3:
		{
			int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
			int offsetReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
			int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[3].name.str, asmBlock, 16, touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%%r%d+%%r%d,%d), %%r%d", movOp, baseReg, offsetReg, ALIGNSIZE(thisTAC->operands[2].name.val), sourceReg));
		}
		break;

		case tt_memw_2_n:
		{
			int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
			int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%%r%d-%d), %%r%d", movOp, baseReg, thisTAC->operands[1].name.val, sourceReg));
		}
		break;

		case tt_memw_3_n:
		{
			int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
			int offsetReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
			int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[3].name.str, asmBlock, 16, touchedRegisters);
			const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s (%%r%d-%%r%d,%d), %%r%d", movOp, baseReg, offsetReg, ALIGNSIZE(thisTAC->operands[2].name.val), sourceReg));
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
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
				int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
				const char *movOp = SelectMovWidth(&thisTAC->operands[1]);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, (%%r%d)", movOp, destReg, sourceReg));
			}
		}
		break;

		case tt_memr_2:
		{
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_memr_2 with spilled destination not supported!\n");
			}
			else
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
				int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
				const char *movOp = SelectMovWidth(&thisTAC->operands[1]);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, (%%r%d+%d)", movOp, destReg, sourceReg, thisTAC->operands[2].name.val));
			}
		}
		break;

		case tt_memr_3:
		{
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_memr_3 with spilled destination not supported!\n");
			}
			else
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
				int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
				int offsetReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2].name.str, asmBlock, 16, touchedRegisters);
				const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, (%%r%d+%%r%d,%d)", movOp, destReg, baseReg, offsetReg, ALIGNSIZE(thisTAC->operands[3].name.val)));
			}
		}
		break;

		case tt_memr_2_n:
		{
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_memr_2 with spilled destination not supported!\n");
			}
			else
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
				int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
				const char *movOp = SelectMovWidth(&thisTAC->operands[1]);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, (%%r%d-%d)", movOp, destReg, sourceReg, thisTAC->operands[2].name.val));
			}
		}
		break;

		case tt_memr_3_n:
		{
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0].name.str);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_memr_3 with spilled destination not supported!\n");
			}
			else
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
				int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1].name.str, asmBlock, reservedRegisters[1], touchedRegisters);
				int offsetReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2].name.str, asmBlock, 16, touchedRegisters);
				const char *movOp = SelectMovWidth(&thisTAC->operands[0]);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d, (%%r%d-%%r%d,%d)", movOp, destReg, baseReg, offsetReg, ALIGNSIZE(thisTAC->operands[3].name.val)));
			}
		}
		break;

		case tt_cmp:
		{
			char immediateInstruction = 0;

			struct Lifetime *operand1Lifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[1].name.str);
			struct Lifetime *operand2Lifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[2].name.str);
			char op1[12];
			char op2[12];

			if (thisTAC->operands[1].permutation == vp_literal && thisTAC->operands[2].permutation == vp_literal)
			{
				ErrorAndExit(ERROR_INTERNAL, "Cmp between two literals!\n");
			}

			// examine the first operand, place it in regisetr if necessary, and handle reordering (if possible/beneficial)
			switch (thisTAC->operands[1].permutation)
			{
			case vp_standard:
			case vp_temp:
				if (!operand1Lifetime->isSpilled)
				{
					sprintf(op1, "%%r%d", operand1Lifetime->stackOrRegLocation);
				}
				else
				{
					placeOrFindOperandInRegister(allLifetimes, operand1Lifetime->variable, asmBlock, reservedRegisters[0], touchedRegisters);
					sprintf(op1, "%%r%d", reservedRegisters[0]);
				}
				break;

			case vp_literal:
				ErrorAndExit(ERROR_INTERNAL, "First operand of comparison is a literal\n");
				break;
			}

			// examine the second operand, place it in register
			switch (thisTAC->operands[2].permutation)
			{
			case vp_standard:
			case vp_temp:
				if (!operand2Lifetime->isSpilled)
				{
					sprintf(op2, "%%r%d", operand2Lifetime->stackOrRegLocation);
				}
				else
				{
					placeOrFindOperandInRegister(allLifetimes, operand2Lifetime->variable, asmBlock, reservedRegisters[1], touchedRegisters);
					sprintf(op2, "%%r%d", reservedRegisters[1]);
				}
				break;

			case vp_literal:
				// TODO:
				// range check immediate values
				// if they don't fit fully, load into register via 2 instructions

				immediateInstruction = 1;
				sprintf(op2, "$%s", thisTAC->operands[2].name.str);
				break;
			}

			if (immediateInstruction)
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%si %s, %s", getAsmOp(thisTAC->operation), op1, op2));
			}
			else
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %s, %s", getAsmOp(thisTAC->operation), op1, op2));
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
			switch (thisTAC->operands[0].permutation)
			{
			case vp_standard:
			case vp_temp:
			{
				int registerIndex = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%r%d", SelectPushWidth(&thisTAC->operands[0]), registerIndex));
			}
			break;

			case vp_literal:
				PlaceLiteralInRegister(asmBlock, thisTAC->operands[0].name.str, "%rr");
				TRIM_APPEND(asmBlock, sprintf(printedLine, "%s %%rr", SelectPushWidth(&thisTAC->operands[0])));
				break;
			}
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
					TRIM_APPEND(asmBlock, sprintf(printedLine, "mov %%r%d, %%rr", returnedLifetime->stackOrRegLocation));
				}
				else
				{
					TRIM_APPEND(asmBlock, sprintf(printedLine, "mov (%%bp+%d), %%rr", returnedLifetime->stackOrRegLocation));
				}
			}
		}
		break;

		case tt_label:
			TRIM_APPEND(asmBlock, sprintf(printedLine, "%s_%d:", functionName, thisTAC->operands[0].name.val));
			break;

		case tt_return:
		{
			switch (thisTAC->operands[0].permutation)
			{
			case vp_literal:
			{
				TRIM_APPEND(asmBlock, sprintf(printedLine, "movh %%rr, $%s", thisTAC->operands[0].name.str));
			}
			break;

			case vp_standard:
			case vp_temp:
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0].name.str, asmBlock, reservedRegisters[0], touchedRegisters);
				TRIM_APPEND(asmBlock, sprintf(printedLine, "mov %%rr, %%r%d", destReg));
			}
			break;

			default:
				perror("unexpected type in return TAC!\n");
			}
			TRIM_APPEND(asmBlock, sprintf(printedLine, "jmp %s_done", functionName));
		}
		break;

		case tt_do:
		case tt_enddo:
			break;
		}
	}
}
