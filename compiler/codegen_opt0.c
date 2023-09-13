#include "codegen_opt0.h"

void generateCodeForProgram_0(struct SymbolTable *table, FILE *outFile, int regAllocOpt)
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
				generateCodeForFunction_0(outFile, generatedFunction, regAllocOpt);

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

			generateCodeForFunction_0(outFile, &start, regAllocOpt);
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
void generateCodeForFunction_0(FILE *outFile, struct FunctionEntry *function, int regAllocOpt)
{

	printf("generate code for function %s", function->name);

	struct CodegenMetadata metadata;
	memset(&metadata, 0, sizeof(struct CodegenMetadata));
	metadata.function = function;
	metadata.reservedRegisters[0] = -1;
	metadata.reservedRegisters[1] = -1;
	metadata.reservedRegisters[2] = -1;
	metadata.reservedRegisterCount = 0;
	int localStackSize = allocateRegisters(&metadata, regAllocOpt);

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
		generateCodeForBasicBlock_0(outFile, block, function->mainScope, metadata.allLifetimes, function->name, metadata.reservedRegisters);
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

void generateCodeForBasicBlock_0(FILE *outFile,
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

		case tt_addrof:
		{
			int addrReg = pickWriteRegister(lifetimes, scope, &thisTAC->operands[0], reservedRegisters[0]);
			addrReg = placeAddrOfLifetimeInReg(outFile, lifetimes, scope, &thisTAC->operands[1], addrReg);
			WriteVariable(outFile, lifetimes, scope, &thisTAC->operands[0], addrReg);
		}
		break;

		case tt_memw_1:
		{
			int destAddrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int sourceReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			const char *movOp = SelectMovWidthForDereference(scope, &thisTAC->operands[0]);
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
			int addrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			const char *movOp = SelectMovWidthForDereference(scope, &thisTAC->operands[1]);
			int destReg = pickWriteRegister(lifetimes, scope, &thisTAC->operands[0], reservedRegisters[1]);

			fprintf(outFile, "\t%s %s, (%s) ; HERE BE BUGS\n",
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
			fprintf(outFile, "\tjmp %s_done\n", scope->parentFunction->name);
		}
		break;

		case tt_do:
		case tt_enddo:
			break;
		}
	}
}
