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
						fprintf(outFile, ".byte ");
						for (int i = 0; i < Scope_getSizeOfArrayElement(table->globalScope, v); i++)
						{
							fprintf(outFile, "0x%02x ", v->type.initializeArrayTo[e][i]);
						}
						fprintf(outFile, "\n");
					}
				}
				else
				{
					fprintf(outFile, ".byte ");
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

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Emitting function prologue\n");
	}
	fprintf(outFile, "%s:\n", function->name);

	// push return address
	EmitPushForSize(outFile, 4, 1);

	// push frame pointer, copy stack pointer to frame pointer
	EmitPushForSize(outFile, 4, 8);
	fprintf(outFile, "mv fp, sp\n");

	if (function->isAsmFun)
	{
		if (currentVerbosity > VERBOSITY_MINIMAL)
		{
			printf("%s is an asm function\n", function->name);
		}

		if (function->BasicBlockList->size != 1)
		{
			ErrorAndExit(ERROR_INTERNAL, "Asm function with %d basic blocks seen - expected 1!\n", function->BasicBlockList->size);
		}

		struct BasicBlock *asmBlock = function->BasicBlockList->head->data;

		for (struct LinkedListNode *asmBlockRunner = asmBlock->TACList->head; asmBlockRunner != NULL; asmBlockRunner = asmBlockRunner->next)
		{
			struct TACLine *asmTAC = asmBlockRunner->data;
			if (asmTAC->operation != tt_asm)
			{
				ErrorWithAST(ERROR_INTERNAL, asmTAC->correspondingTree, "Non-asm TAC type seen in asm function!\n");
			}
			fprintf(outFile, "\t%s\n", asmTAC->operands[0].name.str);
		}

		// pop frame pointer
		EmitPopForSize(outFile, 4, 8);

		// pop return address
		EmitPopForSize(outFile, 4, 1);

		fprintf(outFile, "\taddi sp, sp, %d\n", function->argStackSize);

		fprintf(outFile, "\tjalr zero, 0(%s)\n", registerNames[1]);

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

	if (localStackSize > 0)
	{
		fprintf(outFile, "\taddi sp, sp, -%d\n", localStackSize);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Need %d bytes on stack\n", localStackSize);
	}
	for (int i = MACHINE_REGISTER_COUNT - 1; i >= 0; i--)
	{
		if (metadata.touchedRegisters[i])
		{
			EmitPushForSize(outFile, 4, i);
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

				const char *loadWidth = SelectWidthForLifetime(function->mainScope, thisLifetime);
				fprintf(outFile, "\tl%su %s, %d(fp) # place %s\n",
						loadWidth,
						registerNames[thisLifetime->registerLocation],
						theArgument->stackOffset,
						thisLifetime->name);
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
	for (int i = 0; i < MACHINE_REGISTER_COUNT; i++)
	{
		if (metadata.touchedRegisters[i])
		{
			EmitPopForSize(outFile, 4, i);
		}
	}

	if (localStackSize > 0)
	{
		fprintf(outFile, "\taddi sp, sp, %d\n", localStackSize);
	}

	// pop frame pointer
	EmitPopForSize(outFile, 4, 8);

	// pop return address
	EmitPopForSize(outFile, 4, 1);

	if (function->argStackSize > 0)
	{
		fprintf(outFile, "\taddi sp, sp, %d\n", function->argStackSize);
	}
	fprintf(outFile, "\tjalr zero, 0(%s)\n", registerNames[1]);

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
			fprintf(outFile, "\n\t # %s\n", printedTAC);
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
			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], opLocReg);
		}
		break;

		case tt_add:
		case tt_subtract:
		case tt_mul:
		case tt_div:
		{
			int op1Reg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int op2Reg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);

			fprintf(outFile, "\t%s %s, %s, %s\n", getAsmOp(thisTAC->operation), registerNames[destReg], registerNames[op1Reg], registerNames[op2Reg]);
			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_load:
		{
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[1]);

			const char *loadWidth = SelectWidth(scope, &thisTAC->operands[0]);
			fprintf(outFile, "\tl%su %s, 0(%s)\n",
					loadWidth,
					registerNames[destReg],
					registerNames[baseReg]);

			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_load_off:
		{
			// TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[1]);

			fprintf(outFile, "\tl%su %s, %d(%s)",
					SelectWidth(scope, &thisTAC->operands[0]),
					registerNames[destReg],
					thisTAC->operands[2].name.val,
					registerNames[baseReg]);

			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_load_arr:
		{
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			int offsetReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[2]);

			// TODO: check for shift by 0 and don't shift when applicable
			// perform a left shift by however many bits necessary to scale our value, place the result in reservedRegisters[2]
			fprintf(outFile, "\tslli %s, %s, %d\n",
					registerNames[reservedRegisters[2]],
					registerNames[offsetReg],
					thisTAC->operands[3].name.val);

			// add our scaled offset to the base address, put the full address into reservedRegisters[1]
			fprintf(outFile, "\tadd %s, %s, %s\n",
					registerNames[reservedRegisters[1]],
					registerNames[baseReg],
					registerNames[reservedRegisters[2]]);

			fprintf(outFile, "\tl%su %s, 0(%s)",
					SelectWidthForDereference(scope, &thisTAC->operands[0]),
					registerNames[destReg],
					registerNames[reservedRegisters[0]]);

			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_store:
		{
			int destAddrReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int sourceReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			const char *storeWidth = SelectWidthForDereference(scope, &thisTAC->operands[0]);

			fprintf(outFile, "\ts%s %s, 0(%s)\n",
					storeWidth,
					registerNames[sourceReg],
					registerNames[destAddrReg]);
		}
		break;

		case tt_store_off:
		{
			// TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int sourceReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			fprintf(outFile, "\ts%s %s, %d(%s)",
					SelectWidth(scope, &thisTAC->operands[0]),
					registerNames[sourceReg],
					thisTAC->operands[1].name.val,
					registerNames[baseReg]);
		}
		break;

		case tt_store_arr:
		{
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int offsetReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			int sourceReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[3], reservedRegisters[2]);

			// TODO: check for shift by 0 and don't shift when applicable
			// perform a left shift by however many bits necessary to scale our value, place the result in reservedRegisters[1]
			fprintf(outFile, "\tslli %s, %s, %d\n",
					registerNames[reservedRegisters[1]],
					registerNames[offsetReg],
					thisTAC->operands[2].name.val);

			// add our scaled offset to the base address, put the full address into reservedRegisters[0]
			fprintf(outFile, "\tadd %s, %s, %s\n",
					registerNames[reservedRegisters[0]],
					registerNames[baseReg],
					registerNames[reservedRegisters[1]]);

			fprintf(outFile, "\ts%s %s, 0(%s)\n",
					SelectWidthForDereference(scope, &thisTAC->operands[0]),
					registerNames[sourceReg],
					registerNames[reservedRegisters[0]]);
		}
		break;

		case tt_addrof:
		{
			int addrReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			addrReg = placeAddrOfLifetimeInReg(outFile, scope, lifetimes, &thisTAC->operands[1], addrReg);
			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], addrReg);
		}
		break;

		case tt_lea_off:
		{
			// TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);

			fprintf(outFile, "\taddi %s, %s, %d",
					registerNames[destReg],
					registerNames[baseReg],
					thisTAC->operands[2].name.val);

			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_lea_arr:
		{
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int baseReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			int offsetReg = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[3], reservedRegisters[2]);

			// TODO: check for shift by 0 and don't shift when applicable
			// perform a left shift by however many bits necessary to scale our value, place the result in reservedRegisters[1]
			fprintf(outFile, "\tslli %s, %s, %d\n",
					registerNames[reservedRegisters[2]],
					registerNames[offsetReg],
					thisTAC->operands[2].name.val);

			// add our scaled offset to the base address, put the full address into destReg
			fprintf(outFile, "\tadd %s, %s, %s\n",
					registerNames[destReg],
					registerNames[baseReg],
					registerNames[reservedRegisters[2]]);

			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_beq:
		case tt_bne:
		case tt_bgeu:
		case tt_bltu:
		case tt_bgtu:
		case tt_bleu:
		case tt_beqz:
		case tt_bnez:
		{
			int operand1register = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int operand2register = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			fprintf(outFile, "\t%s %s, %s, %s_%d\n",
					getAsmOp(thisTAC->operation),
					registerNames[operand1register],
					registerNames[operand2register],
					functionName,
					thisTAC->operands[0].name.val);
		}
		break;

		case tt_jmp:
		{
			fprintf(outFile, "\tj %s_%d\n", functionName, thisTAC->operands[0].name.val);
		}
		break;

		case tt_push:
		{
			int operandRegister = placeOrFindOperandInRegister(outFile, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			EmitPushForOperand(outFile, scope, &thisTAC->operands[0], operandRegister);
		}
		break;

		case tt_pop:
		{
			int operandRegister = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			EmitPopForOperand(outFile, scope, &thisTAC->operands[0], operandRegister);
			WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], operandRegister);
		}
		break;

		case tt_call:
		{
			fprintf(outFile, "\tjal ra, %s\n", thisTAC->operands[1].name.str);

			if (thisTAC->operands[0].name.str != NULL)
			{
				WriteVariable(outFile, scope, lifetimes, &thisTAC->operands[0], RETURN_REGISTER);
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
					fprintf(outFile, "\tmv %s, %s\n",
							registerNames[RETURN_REGISTER],
							registerNames[sourceReg]);
				}
			}
			fprintf(outFile, "\tj %s_done\n", scope->parentFunction->name);
		}
		break;

		case tt_do:
		case tt_enddo:
			break;
		}
	}
}
