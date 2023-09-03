#include "regalloc.h"

struct Lifetime *newLifetime(char *name, enum variableTypes type, int indirectionLevel, int start, char isGlobal)
{
	struct Lifetime *wip = malloc(sizeof(struct Lifetime));
	wip->name = name;
	wip->type = type;
	wip->indirectionLevel = indirectionLevel;
	wip->start = start;
	wip->end = start;
	wip->stackOrRegLocation = -1;
	wip->type = type;
	wip->nwrites = 0;
	wip->nreads = 0;
	if (isGlobal)
	{
		wip->isSpilled = 1;
	}
	else
	{
		wip->isSpilled = 0;
	}
	wip->isArgument = 0;
	wip->isGlobal = isGlobal;
	wip->localPointerTo = NULL;
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
										enum variableTypes type,
										int indirectionLevel,
										int newEnd,
										char isGlobal)
{
	struct Lifetime *thisLt = LinkedList_Find(ltList, &compareLifetimes, name);

	if (thisLt != NULL)
	{
		// this should never fire with well-formed TAC
		// may be helpful when adding/troubleshooting new TAC generation
		if (thisLt->type != type)
		{
			ErrorAndExit(ERROR_INTERNAL, "Error - type mismatch between identically named variables [%s] expected %d, saw %d!\n", name, type, type);
		}
		if (newEnd > thisLt->end)
			thisLt->end = newEnd;
	}
	else
	{
		thisLt = newLifetime(name, type, indirectionLevel, newEnd, isGlobal);
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
	struct Lifetime *updatedLifetime = updateOrInsertLifetime(ltList, writtenOperand->name.str, writtenOperand->type, writtenOperand->indirectionLevel, newEnd, isGlobal);
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
	struct Lifetime *updatedLifetime = updateOrInsertLifetime(ltList, readOperand->name.str, readOperand->type, readOperand->indirectionLevel, newEnd, isGlobal);
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
			struct Lifetime *argLifetime = updateOrInsertLifetime(lifetimes, thisMember->name, theArgument->type, theArgument->indirectionLevel, 1, 0);
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
				updateOrInsertLifetime(lifetimes, declared->name.str, declared->type, declared->indirectionLevel, TACIndex, 0);
			}
			break;

			case tt_call:
				if (thisLine->operands[0].type != vt_null)
				{
					recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
				}
				break;

			case tt_assign:
			case tt_cast_assign:
			{
				recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
				switch(thisLine->operands[1].permutation)
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
				switch(thisLine->operands[0].permutation)
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
				if (thisLine->operands[0].type != vt_null)
				{
					recordVariableWrite(lifetimes, &thisLine->operands[0], scope, TACIndex);
				}

				for (int i = 1; i < 4; i++)
				{
					// lifetimes for every permutation except literal
					if (thisLine->operands[i].permutation != vp_literal)
					{
						// and any type except null
						switch (thisLine->operands[i].type)
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
						switch (thisLine->operands[i].type)
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

		// if this lifetime must be spilled (has address-of operator used) (for future use - nothing currently uses(?)), add directly to the spilled list
		struct ScopeMember *thisVariableEntry = Scope_lookup(metadata->function->mainScope, thisLifetime->name);

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
			// if (thisVariableEntry != NULL && ((struct VariableEntry *)thisVariableEntry->entry)->localPointerTo != NULL)
			// {
				// thisLifetime->localPointerTo = ((struct VariableEntry *)thisVariableEntry->entry)->localPointerTo;
				// thisLifetime->stackOrRegLocation = -1;
				// Stack_Push(metadata->localPointerLifetimes, thisLifetime);
			// }
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

void spillVariables(struct CodegenMetadata *metadata, int mostConcurrentLifetimes)
{
	// always keep 1 scratch register (for literal loading for example)
	int MAXREG = REGISTERS_TO_ALLOCATE - 1;
	metadata->reservedRegisters[0] = SCRATCH_REGISTER;

	// if we have just enough room, simply use all registers
	// if we need to spill, ensure 2 scratch registers
	if (mostConcurrentLifetimes > MAXREG)
	{
		MAXREG -= 2;
		metadata->reservedRegisters[1] = SECOND_SCRATCH_REGISTER;
		metadata->reservedRegisters[2] = RETURN_REGISTER;
	}
	else
	{
		return;
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
				if (thisHeuristic < bestHeuristic)
				{
					bestHeuristic = thisHeuristic;
					bestLifetime = thisLifetime;
				}
			}

			// this method actually deletes the spilled variable from the liveness array
			// if it becomes necessary to keep the untouched liveness array around, it will need to be copied within this function
			for (int j = bestLifetime->start; j <= bestLifetime->end; j++)
			{
				LinkedList_Delete(metadata->lifetimeOverlaps[j], compareLifetimes, bestLifetime->name);
			}
			bestLifetime->isSpilled = 1;
			Stack_Push(metadata->spilledLifetimes, bestLifetime);
		}
	}
}

// sort the list of spilled lifetimes by size of the variable so they can be laid out cleanly on the stack
void sortSpilledLifetimes(struct CodegenMetadata *metadata)
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

			int thisSize = GetSizeOfPrimitive(thisLifetime->type);
			int compSize = GetSizeOfPrimitive(((struct Lifetime *)metadata->spilledLifetimes->data[j + 1])->type);

			if (thisSize > compSize)
			{
				struct Lifetime *swap = metadata->spilledLifetimes->data[j];
				metadata->spilledLifetimes->data[j] = metadata->spilledLifetimes->data[j + 1];
				metadata->spilledLifetimes->data[j + 1] = swap;
			}
		}
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

	// reserve scratch registers for arithmetic
	// always will have one reserved register
	registers[metadata->reservedRegisters[0]] = 1;
	metadata->touchedRegisters[metadata->reservedRegisters[0]] = 1;

	// if we have a second, mark that off as well
	if (metadata->reservedRegisters[1] > 0)
	{
		registers[metadata->reservedRegisters[1]] = 1;
		metadata->touchedRegisters[metadata->reservedRegisters[1]] = 1;
	}

	for (int i = 0; i <= metadata->largestTacIndex; i++)
	{
		// free any registers inhabited by expired lifetimes
		for (int j = 0; j < REGISTERS_TO_ALLOCATE; j++)
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
				for (int j = 0; j < REGISTERS_TO_ALLOCATE; j++)
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

					ErrorAndExit(ERROR_INTERNAL, "Unable to find register for variable %s!\n", thisLifetime->name);
				}
			}
		}
	}
}
