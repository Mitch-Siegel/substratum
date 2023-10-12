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
			struct BasicBlock *thisBlock = thisMember->entry;
			if (thisBlock->labelNum == 0)
			{
				if (thisBlock->TACList->size == 0)
				{
					break;
				}

				fprintf(outFile, "~export section userstart\n");
				struct LinkedList *fakeBlockList = LinkedList_New();
				LinkedList_Append(fakeBlockList, thisBlock);

				struct LinkedList *globalLifetimes = findLifetimes(table->globalScope, fakeBlockList);
				LinkedList_Free(fakeBlockList, NULL);
				int reserved[3] = {0, 1, 2};

				generateCodeForBasicBlock_0(outFile, thisBlock, table->globalScope, globalLifetimes, NULL, reserved);
				LinkedList_Free(globalLifetimes, free);
				fprintf(outFile, "~end export section userstart\n");
			}
			else if (thisBlock->labelNum == 1)
			{
				fprintf(outFile, "~export section asm\n");

				for (struct LinkedListNode *examinedLine = thisBlock->TACList->head; examinedLine != NULL; examinedLine = examinedLine->next)
				{
					struct TACLine *examinedTAC = examinedLine->data;
					if (examinedTAC->operation != tt_asm)
					{
						ErrorAndExit(ERROR_INTERNAL, "Unexpected TAC type %d (%s) seen in global ASM block!\n",
									 examinedTAC->operation,
									 getAsmOp(examinedTAC->operation));
					}
					fprintf(outFile, "%s\n", examinedTAC->operands[0].name.str);
				}
				fprintf(outFile, "~end export section asm\n");
			}
			else
			{
				ErrorAndExit(ERROR_INTERNAL, "Unexpected basic block index %d at global scope!\n", thisBlock->labelNum);
			}

			/*
			struct BasicBlock *thisBlock = (struct BasicBlock *)thisMember->entry;
			fprintf(outFile, "~export asm\n");

			for (struct LinkedListNode *asmLine = thisBlock->TACList->head; asmLine != NULL; asmLine = asmLine->next)
			{
				struct TACLine *asmTAC = (struct TACLine *)asmLine->data;
				if (asmTAC->operation != tt_asm)
				{
					ErrorAndExit(ERROR_INTERNAL, "Expected only asm at global scope, got %s instead!\n", getAsmOp(asmTAC->operation));
				}
				fprintf(outFile, "%s\n", asmTAC->operands[0].name.str);
			}

			fprintf(outFile, "~end export asm\n");*/
		}
		break;

		case e_variable:
		{
			struct VariableEntry *v = thisMember->entry;
			fprintf(outFile, "~export variable %s\n", thisMember->name);
			char *typeName = Type_GetName(&v->type);
			fprintf(outFile, "%s\n", typeName);
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
extern struct Config config;
void generateCodeForFunction_0(FILE *outFile, struct FunctionEntry *function, int regAllocOpt)
{
	currentVerbosity = config.stageVerbosities[STAGE_CODEGEN];
	if (currentVerbosity > VERBOSITY_SILENT)
	{
		printf("Generate code for function %s\n", function->name);
	}

	if(function->isAsmFun)
	{
		if(currentVerbosity > VERBOSITY_MINIMAL)
		{
			printf("%s is an asm function\n", function->name);
		}
		fprintf(outFile, "%s:\n", function->name);

		if(function->BasicBlockList->size != 1)
		{
			ErrorAndExit(ERROR_INTERNAL, "Asm function with %d basic blocks seen - expected 1!\n", function->BasicBlockList->size);
		}

		struct BasicBlock *asmBlock = function->BasicBlockList->head->data;

		for(struct LinkedListNode *asmBlockRunner = asmBlock->TACList->head; asmBlockRunner != NULL; asmBlockRunner = asmBlockRunner->next)
		{
			struct TACLine *asmTAC = asmBlockRunner->data;
			if(asmTAC->operation != tt_asm)
			{
				ErrorWithAST(ERROR_INTERNAL, asmTAC->correspondingTree, "Non-asm TAC type seen in asm function!\n");
			}
			fprintf(outFile, "\t%s\n", asmTAC->operands[0].name.str);
		}
		fprintf(outFile, "\tret %d\n", function->argStackSize);
		
		// early return, nothing else to do
		return;
	}

	struct CodegenMetadata metadata;
	memset(&metadata, 0, sizeof(struct CodegenMetadata));
	metadata.function = function;
	metadata.reservedRegisters[0] = -1;
	metadata.reservedRegisters[1] = -1;
	metadata.reservedRegisters[2] = -1;
	metadata.reservedRegisterCount = 0;
	currentVerbosity = config.stageVerbosities[STAGE_REGALLOC];
	int localStackSize = allocateRegisters(&metadata, regAllocOpt);
	currentVerbosity = config.stageVerbosities[STAGE_CODEGEN];

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Need %d bytes on stack\n", localStackSize);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Emitting function prologue\n");
	}
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

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Arguments placed into registers\n");
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

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Generating code  for basic blocks\n");
	}

	for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
	{
		struct BasicBlock *block = blockRunner->data;
		generateCodeForBasicBlock_0(outFile, block, function->mainScope, metadata.allLifetimes, function->name, metadata.reservedRegisters);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Emitting function epilogue\n");
	}

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

	LinkedList_Free(metadata.allLifetimes, free);

	for (int i = 0; i <= metadata.largestTacIndex; i++)
	{
		LinkedList_Free(metadata.lifetimeOverlaps[i], NULL);
	}
	free(metadata.lifetimeOverlaps);
}

void generateCodeForBasicBlock_0(FILE *outFile,
								 struct BasicBlock *block,
								 struct Scope *scope,
								 struct LinkedList *lifetimes,
								 char *functionName,
								 int reservedRegisters[3])
{
	// we may pass null if we are generating the code to initialize global variables
	if (functionName != NULL)
	{
		fprintf(outFile, "%s_%d:\n", functionName, block->labelNum);
	}

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

			const char *movOp = SelectMovWidth(scope, &thisTAC->operands[2]);

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

		case tt_lea_2:
		{
			int addrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int destReg = pickWriteRegister(lifetimes, scope, &thisTAC->operands[0], reservedRegisters[1]);
			fprintf(outFile, "\tlea %s, (%s+%d)\n",
					registerNames[destReg],
					registerNames[addrReg],
					thisTAC->operands[2].name.val);

			WriteVariable(outFile, lifetimes, scope, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_lea_3:
		{
			int addrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int offsetReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			int destReg = pickWriteRegister(lifetimes, scope, &thisTAC->operands[0], reservedRegisters[1]);
			fprintf(outFile, "\tlea %s, (%s+%s,%d)\n",
					registerNames[destReg],
					registerNames[addrReg],
					registerNames[offsetReg],
					ALIGNSIZE(thisTAC->operands[3].name.val));
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
