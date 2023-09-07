#include "regalloc.h"

struct Lifetime *newLifetime(char *name, struct Type *type, int start, char isGlobal)
{
	struct Lifetime *wip = malloc(sizeof(struct Lifetime));
	wip->name = name;
	wip->type = *type;
	wip->start = start;
	wip->end = start;
	wip->stackLocation = 0;
	wip->inRegister = 0;
	wip->nwrites = 0;
	wip->nreads = 0;
	wip->mustSpill = 0;
	wip->isGlobal = isGlobal;
	return wip;
}

int compareLifetimes(struct Lifetime *a, char *variable)
{
	return strcmp(a->name, variable);
}

// search through the list of existing lifetimes
// update the lifetime if it exists, insert if it doesn't
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct LinkedList *ltList,
										char *name,
										struct Type *type,
										int newEnd,
										char isGlobal)
{
	struct Lifetime *thisLt = LinkedList_Find(ltList, &compareLifetimes, name);

	if (thisLt != NULL)
	{
		// this should never fire with well-formed TAC
		// may be helpful when adding/troubleshooting new TAC generation
		if (Type_Compare(&thisLt->type, type))
		{
			char *expectedTypeName = Type_GetName(&thisLt->type);
			char *typeName = Type_GetName(type);
			ErrorAndExit(ERROR_INTERNAL, "Error - type mismatch between identically named variables [%s] expected %s, saw %s!\n", name, expectedTypeName, typeName);
		}
		if (newEnd > thisLt->end)
			thisLt->end = newEnd;
	}
	else
	{
		thisLt = newLifetime(name, type, newEnd, isGlobal);
		LinkedList_Append(ltList, thisLt);
	}

	return thisLt;
}

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct LinkedList *ltList,
						 struct TACOperand *writtenOperand,
						 struct Scope *scope,
						 int newEnd)
{
	char isGlobal = 0;
	if (writtenOperand->permutation == vp_standard)
	{
		struct VariableEntry *recordedVariable = Scope_lookupVarByString(scope, writtenOperand->name.str);
		isGlobal = recordedVariable->isGlobal;
	}

	// always use ->type as we don't care what it's cast as to determine its lifetime
	struct Lifetime *updatedLifetime = updateOrInsertLifetime(ltList, writtenOperand->name.str, &writtenOperand->type, newEnd, isGlobal);
	updatedLifetime->nwrites += 1;
}

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
						struct TACOperand *readOperand,
						struct Scope *scope,
						int newEnd)
{
	char isGlobal = 0;
	if (readOperand->permutation == vp_standard)
	{
		struct VariableEntry *recordedVariable = Scope_lookupVarByString(scope, readOperand->name.str);
		isGlobal = recordedVariable->isGlobal;
	}

	// always use ->type as we don't care what it's cast as to determine its lifetime
	struct Lifetime *updatedLifetime = updateOrInsertLifetime(ltList, readOperand->name.str, &readOperand->type, newEnd, isGlobal);
	updatedLifetime->nreads += 1;
}

struct LinkedList *findLifetimes(struct Scope *scope, struct LinkedList *basicBlockList)
{
	struct LinkedList *lifetimes = LinkedList_New();
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		if (thisMember->type == e_argument)
		{
			struct VariableEntry *theArgument = thisMember->entry;
			struct Lifetime *argLifetime = updateOrInsertLifetime(lifetimes, thisMember->name, &theArgument->type, 1, 0);
			argLifetime->isArgument = 1;
		}
	}

	struct LinkedListNode *blockRunner = basicBlockList->head;
	struct Stack *doDepth = Stack_New();
	while (blockRunner != NULL)
	{
		struct BasicBlock *thisBlock = blockRunner->data;
		struct LinkedListNode *TACRunner = thisBlock->TACList->head;
		while (TACRunner != NULL)
		{
			struct TACLine *thisLine = TACRunner->data;
			int TACIndex = thisLine->index;

			switch (thisLine->operation)
			{
			case tt_do:
				Stack_Push(doDepth, (void *)(long int)thisLine->index);
				break;

			case tt_enddo:
			{
				int extendTo = thisLine->index;
				int extendFrom = (int)(long int)Stack_Pop(doDepth);
				for (struct LinkedListNode *lifetimeRunner = lifetimes->head; lifetimeRunner != NULL; lifetimeRunner = lifetimeRunner->next)
				{
					struct Lifetime *examinedLifetime = lifetimeRunner->data;
					if (examinedLifetime->end >= extendFrom && examinedLifetime->end < extendTo)
					{
						if (examinedLifetime->name[0] != '.')
						{
							examinedLifetime->end = extendTo + 1;
						}
					}
				}
			}
			break;

			case tt_asm:
				break;

			case tt_declare:
			{
				struct TACOperand *declared = &thisLine->operands[0];
				updateOrInsertLifetime(lifetimes, declared->name.str, &declared->type, TACIndex, 0);
			}
			break;

			case tt_call:
				if (TAC_GetTypeOfOperand(thisLine, 0)->basicType != vt_null)
				{
					recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
				}
				break;

			case tt_assign:
			case tt_cast_assign:
			{
				recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
				switch (thisLine->operands[1].permutation)
				{
				case vp_standard:
				case vp_temp:
					recordVariableRead(lifetimes, &thisLine->operands[1], scope, TACIndex);
					break;

				case vp_literal:
				case vp_objptr:
					break;
				}
			}
			break;

			// single operand in slot 0
			case tt_push:
			case tt_return:
			{
				switch (thisLine->operands[0].permutation)
				{
				case vp_standard:
				case vp_temp:
					recordVariableRead(lifetimes, &thisLine->operands[0], scope, TACIndex);
					break;

				case vp_literal:
				case vp_objptr:
					break;
				}
			}
			break;

			case tt_add:
			case tt_subtract:
			case tt_mul:
			case tt_div:
			case tt_cmp:
			{
				if (TAC_GetTypeOfOperand(thisLine, 0)->basicType != vt_null)
				{
					recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
				}

				for (int i = 1; i < 4; i++)
				{
					// lifetimes for every permutation except literal
					if (thisLine->operands[i].permutation != vp_literal)
					{
						// and any type except null
						switch (TAC_GetTypeOfOperand(thisLine, i)->basicType)
						{
						case vt_null:
							break;

						default:
							recordVariableRead(lifetimes, &thisLine->operands[i], scope, TACIndex);
							break;
						}
					}
				}
			}
			break;

			case tt_memr_1:
			case tt_memr_2:
			case tt_memr_2_n:
			case tt_memr_3:
			case tt_memr_3_n:
			case tt_memw_1:
			case tt_memw_2:
			case tt_memw_2_n:
			case tt_memw_3:
			case tt_memw_3_n:
			{
				for (int i = 0; i < 4; i++)
				{
					// lifetimes for every permutation except literal
					if (thisLine->operands[i].permutation != vp_literal)
					{
						// and any type except null
						switch (TAC_GetTypeOfOperand(thisLine, i)->basicType)
						{
						case vt_null:
							break;

						default:
							recordVariableRead(lifetimes, &thisLine->operands[i], scope, TACIndex);
							break;
						}
					}
				}
			}
			break;

			case tt_dereference:
			case tt_reference:
			case tt_jg:
			case tt_jge:
			case tt_jl:
			case tt_jle:
			case tt_je:
			case tt_jne:
			case tt_jz:
			case tt_jnz:
			case tt_jmp:
			case tt_label:
				break;
			}
			TACRunner = TACRunner->next;
		}
		blockRunner = blockRunner->next;
	}

	Stack_Free(doDepth);

	return lifetimes;
}

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

// populate a linkedlist array so that the list at index i contains all lifetimes active at TAC index i
// then determine which variables should be spilled
int generateLifetimeOverlaps(struct CodegenMetadata *metadata)
{
	int mostConcurrentLifetimes = 0;

	// populate the array of active lifetimes
	for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;

		for (int i = thisLifetime->start; i <= thisLifetime->end; i++)
		{
			LinkedList_Append(metadata->lifetimeOverlaps[i], thisLifetime);
			if (metadata->lifetimeOverlaps[i]->size > mostConcurrentLifetimes)
			{
				mostConcurrentLifetimes = metadata->lifetimeOverlaps[i]->size;
			}
		}
	}

	return mostConcurrentLifetimes;
}

// return the heuristic for how good a given lifetime is to spill - lower is better
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
	if (!lt->isArgument)
	{
		h *= 10;
	}

	return h;
}

// void spillVariables(struct CodegenMetadata *metadata, int mostConcurrentLifetimes)
// {
// 	// always keep 1 scratch register (for literal loading for example)
// 	int MAXREG = REGISTERS_TO_ALLOCATE - 1;
// 	metadata->reservedRegisters[0] = SCRATCH_REGISTER;

// 	// if we have just enough room, simply use all registers
// 	// if we need to spill, ensure 2 scratch registers
// 	if (mostConcurrentLifetimes > MAXREG)
// 	{
// 		MAXREG -= 2;
// 		metadata->reservedRegisters[1] = SECOND_SCRATCH_REGISTER;
// 		metadata->reservedRegisters[2] = RETURN_REGISTER;
// 	}
// 	else
// 	{
// 		return;
// 	}

// 	// look through the populated array of active lifetimes
// 	// if a given index has too many active lifetimes, figure out which lifetime(s) to spill
// 	// then allocate registers for any lifetimes without a home
// 	for (int i = 0; i <= metadata->largestTacIndex; i++)
// 	{
// 		while (calculateRegisterLoading(metadata->lifetimeOverlaps[i], i) > MAXREG)
// 		{
// 			struct LinkedListNode *overlapRunner = metadata->lifetimeOverlaps[i]->head;

// 			// start off the best heuristic as the first item
// 			struct Lifetime *bestLifetime = (struct Lifetime *)overlapRunner->data;
// 			int bestHeuristic = lifetimeHeuristic(bestLifetime);

// 			for (; overlapRunner != NULL; overlapRunner = overlapRunner->next)
// 			{
// 				struct Lifetime *thisLifetime = overlapRunner->data;

// 				int thisHeuristic = lifetimeHeuristic(thisLifetime);

// 				// printf("%s has heuristic of %f\n", thisLifetime->variable, thisHeuristic);
// 				if (thisHeuristic < bestHeuristic)
// 				{
// 					bestHeuristic = thisHeuristic;
// 					bestLifetime = thisLifetime;
// 				}
// 			}

// 			// this method actually deletes the spilled variable from the liveness array
// 			// if it becomes necessary to keep the untouched liveness array around, it will need to be copied within this function
// 			for (int j = bestLifetime->start; j <= bestLifetime->end; j++)
// 			{
// 				LinkedList_Delete(metadata->lifetimeOverlaps[j], compareLifetimes, bestLifetime->name);
// 			}
// 			bestLifetime->isSpilled = 1;
// 			Stack_Push(metadata->spilledLifetimes, bestLifetime);
// 		}
// 	}
// }

// // sort the list of spilled lifetimes by size of the variable so they can be laid out cleanly on the stack
// void sortSpilledLifetimes(struct CodegenMetadata *metadata)
// {
// 	// TODO: handle arrays properly?
// 	//  - this shouldn't be a consideration because the only lifetimes that are considered to be spilled are localpointers?
// 	//  - but localpointers will still be in the spilled list if they aren't being kept in registers, because they need somewhere to be
// 	//     despite the fact that they really don't actually be spilled to stack, instead just generated from base pointer and offset

// 	// simple bubble sort
// 	for (int i = 0; i < metadata->spilledLifetimes->size; i++)
// 	{
// 		for (int j = 0; j < metadata->spilledLifetimes->size - i - 1; j++)
// 		{
// 			struct Lifetime *thisLifetime = metadata->spilledLifetimes->data[j];

// 			int thisSize = Scope_getSizeOfType(metadata->function->mainScope, &thisLifetime->type);
// 			int compSize = Scope_getSizeOfType(metadata->function->mainScope, &(((struct Lifetime *)metadata->spilledLifetimes->data[j + 1])->type));

// 			if (thisSize > compSize)
// 			{
// 				struct Lifetime *swap = metadata->spilledLifetimes->data[j];
// 				metadata->spilledLifetimes->data[j] = metadata->spilledLifetimes->data[j + 1];
// 				metadata->spilledLifetimes->data[j + 1] = swap;
// 			}
// 		}
// 	}
// }

void selectRegisterVariables(struct CodegenMetadata *metadata, int mostConcurrentLifetimes)
{
	int MAXREG = REGISTERS_TO_ALLOCATE - 2;
	metadata->reservedRegisters[0] = SCRATCH_REGISTER;
	metadata->reservedRegisters[1] = RETURN_REGISTER;
	metadata->reservedRegisterCount = 2;

	// 	// if we have just enough room, simply use all registers
	// 	// if we need to spill, ensure 2 scratch registers
	if (mostConcurrentLifetimes > MAXREG)
	{
		MAXREG -= 1;
		metadata->reservedRegisters[2] = SECOND_SCRATCH_REGISTER;
		metadata->reservedRegisterCount++;
	}

	metadata->registerLifetimes = LinkedList_New();

	// iterate all TAC indices, remove variables we must spill from contention for a register
	for (int tacIndex = 0; tacIndex < metadata->largestTacIndex; tacIndex++)
	{
		struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
		for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
		{
			struct Lifetime *examinedLifetime = runner->data;

			if (examinedLifetime->mustSpill)
			{
				// remove (from all indices) the lifetime we will no longer consider for a register
				for (int removeTacIndex = 0; removeTacIndex <= metadata->largestTacIndex; removeTacIndex++)
				{
					if (LinkedList_Find(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, examinedLifetime->name) != NULL)
					{
						LinkedList_Delete(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, examinedLifetime->name);
					}
				}
			}
		}
	}

	// iterate all TAC indices
	for (int tacIndex = 0; tacIndex < metadata->largestTacIndex; tacIndex++)
	{
		struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
		// while too many lifetimes are in contention for a register at this index
		while (activeLifetimesThisIndex->size > MAXREG)
		{
			// loop over all lifetimes, find the one with the best heuristic to spill
			struct Lifetime *bestToSpill = NULL;
			int bestHeuristic = __INT_MAX__;
			for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
			{
				struct Lifetime *examinedLifetime = runner->data;
				int thisHeuristic = lifetimeHeuristic(examinedLifetime);
				if (thisHeuristic < bestHeuristic)
				{
					bestToSpill = examinedLifetime;
					bestHeuristic = thisHeuristic;
				}
			}

			// remove (from all indices) the lifetime we will no longer consider for a register
			for (int removeTacIndex = 0; removeTacIndex <= metadata->largestTacIndex; removeTacIndex++)
			{
				if (LinkedList_Find(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, bestToSpill->name) != NULL)
				{
					LinkedList_Delete(metadata->lifetimeOverlaps[removeTacIndex], compareLifetimes, bestToSpill->name);
				}
			}
		}

		for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
		{
			struct Lifetime *addedLifetime = runner->data;

			if (LinkedList_Find(metadata->registerLifetimes, compareLifetimes, addedLifetime->name) == NULL)
			{
				LinkedList_Append(metadata->registerLifetimes, addedLifetime);
			}
		}
	}

	for (int tacIndex = 0; tacIndex <= metadata->largestTacIndex; tacIndex++)
	{
		struct LinkedList *activeLifetimesThisIndex = metadata->lifetimeOverlaps[tacIndex];
		printf("%2d:\t", tacIndex);
		for (struct LinkedListNode *runner = activeLifetimesThisIndex->head; runner != NULL; runner = runner->next)
		{
			struct Lifetime *printed = runner->data;
			printf("%s ", printed->name);
		}
		printf("\n");
	}
}

void assignRegisters(struct CodegenMetadata *metadata)
{
	// printf("\nassigning registers\n");
	// flag registers in use at any given TAC index so we can easily assign
	char registers[REGISTERS_TO_ALLOCATE];
	struct Lifetime *occupiedBy[REGISTERS_TO_ALLOCATE];

	for (int i = 0; i < REGISTERS_TO_ALLOCATE; i++)
	{
		registers[i] = 0;
		occupiedBy[i] = NULL;
		metadata->touchedRegisters[i] = 0;
	}

	printf("have %d reserved registers\n", metadata->reservedRegisterCount);
	for (int i = 0; i <= metadata->largestTacIndex; i++)
	{
		// free any registers inhabited by expired lifetimes
		for (int j = metadata->reservedRegisterCount; j < REGISTERS_TO_ALLOCATE; j++)
		{
			if (occupiedBy[j] != NULL && occupiedBy[j]->end <= i)
			{
				printf("%s expires at %d\n", occupiedBy[j]->name, i);
				registers[j] = 0;
				occupiedBy[j] = NULL;
			}
		}

		// iterate all lifetimes and assign newly-live ones to a register
		for (struct LinkedListNode *ltRunner = metadata->registerLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
		{
			struct Lifetime *thisLifetime = ltRunner->data;
			if ((thisLifetime->start == i) && // if the lifetime starts at this step
				(!thisLifetime->inRegister))  // lifetime doesn't yet have a register
			{
				char registerFound = 0;
				// scan through all registers, looking for an unoccupied one
				for (int j = metadata->reservedRegisterCount; j < REGISTERS_TO_ALLOCATE; j++)
				{
					if (registers[j] == 0)
					{
						printf("\tAssign register %d for variable %s\n", j, thisLifetime->name);
						thisLifetime->registerLocation = j;
						thisLifetime->inRegister = 1;
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

					ErrorAndExit(ERROR_INTERNAL, "Unable to find register for variable %s!\n", thisLifetime->name);
				}
			}
		}
	}

	LinkedList_Free(metadata->registerLifetimes, NULL);
	metadata->registerLifetimes = NULL;
}

void assignStackSpace(struct LinkedList *allLifetimes)
{
	struct Stack *needStackSpace = Stack_New();
	for (struct LinkedListNode *runner = allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *examined = runner->data;
		if (!examined->inRegister)
		{
			if (!examined->isGlobal)
			{
				Stack_Push(needStackSpace, examined);
			}
		}
	}

	printf("%d variables need stack space\n", needStackSpace->size);

	Stack_Free(needStackSpace);
}

void allocateRegisters(struct CodegenMetadata *metadata)
{
	metadata->allLifetimes = findLifetimes(metadata->function->mainScope, metadata->function->BasicBlockList);

	// find all overlapping lifetimes, to figure out which variables can live in registers vs being spilled
	metadata->largestTacIndex = 0;
	for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		if (thisLifetime->end > metadata->largestTacIndex)
		{
			metadata->largestTacIndex = thisLifetime->end;
		}
	}

	printf("\nLifetimes for %s\n", metadata->function->name);
	for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *examinedLifetime = runner->data;
		printf("%25s (%2d-%2d): ", examinedLifetime->name, examinedLifetime->start, examinedLifetime->end);
		for (int i = 0; i <= metadata->largestTacIndex; i++)
		{
			if (i >= examinedLifetime->start && i <= examinedLifetime->end)
			{
				printf("*");
			}
			else
			{
				printf(" ");
			}
		}
		printf("\n");
	}

	// generate an array of lists corresponding to which lifetimes are active at a given TAC step by index in the array
	metadata->lifetimeOverlaps = malloc((metadata->largestTacIndex + 1) * sizeof(struct LinkedList *));
	for (int i = 0; i <= metadata->largestTacIndex; i++)
	{
		metadata->lifetimeOverlaps[i] = LinkedList_New();
	}

	int mostConcurrentLifetimes = generateLifetimeOverlaps(metadata);

	printf("at most %d concurrent lifetimes\n", mostConcurrentLifetimes);

	selectRegisterVariables(metadata, mostConcurrentLifetimes);

	assignRegisters(metadata);

	assignStackSpace(metadata->allLifetimes);

	for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *examined = runner->data;
		if (examined->inRegister)
		{
			printf("%20s: %%r%d\n", examined->name, examined->registerLocation);
		}
		else
		{
			printf("%20s: SPILLME!\n", examined->name);
		}
	}
}
