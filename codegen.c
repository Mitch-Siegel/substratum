#include "codegen.h"
#include "codegen_generic.h"

void generateCodeForProgram(struct SymbolTable *table, FILE *outFile)
{
	struct CodegenContext globalContext;
	int globalInstructionIndex = 0;
	globalContext.instructionIndex = &globalInstructionIndex;
	globalContext.outFile = outFile;

	// fprintf(outFile, "\t.text\n");
	for (int i = 0; i < table->globalScope->entries->size; i++)
	{
		struct ScopeMember *thisMember = table->globalScope->entries->data[i];
		switch (thisMember->type)
		{
		case e_function:
		{
			struct FunctionEntry *generatedFunction = thisMember->entry;
			if (!generatedFunction->isDefined)
			{
				break;
			}
			if (!strcmp(generatedFunction->name, "main"))
			{
				fprintf(outFile, "\t.globl _start\n_start:\n\tli sp, 0x81000000\n\tcall main\n\tpgm_done:\n\twfi\n\tj pgm_done\n");
			}

			fprintf(outFile, "\t.globl %s\n", generatedFunction->name);
			fprintf(outFile, "\t.type %s, @function\n", generatedFunction->name);

			generateCodeForFunction(outFile, generatedFunction);
			fprintf(outFile, "\t.size %s, .-%s\n", generatedFunction->name, generatedFunction->name);
		}
		break;

		case e_basicblock:
		{
			struct BasicBlock *thisBlock = thisMember->entry;
			if (thisBlock->TACList->size == 0)
			{
				break;
			}

			// compiled code
			if (thisBlock->labelNum == 0)
			{
				fprintf(outFile, ".userstart:\n");
				struct LinkedList *fakeBlockList = LinkedList_New();
				LinkedList_Append(fakeBlockList, thisBlock);

				struct LinkedList *globalLifetimes = findLifetimes(table->globalScope, fakeBlockList);
				LinkedList_Free(fakeBlockList, NULL);
				int reserved[3] = {0, 1, 2};

				generateCodeForBasicBlock(&globalContext, thisBlock, table->globalScope, globalLifetimes, NULL, reserved);
				LinkedList_Free(globalLifetimes, free);
			} // assembly block
			else if (thisBlock->labelNum == 1)
			{

				fprintf(outFile, ".rawasm:\n");

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
			}
			else
			{
				ErrorAndExit(ERROR_INTERNAL, "Unexpected basic block index %d at global scope!\n", thisBlock->labelNum);
			}
		}
		break;

		case e_variable:
		{
			struct VariableEntry *variable = thisMember->entry;

			// early break if the variable is declared as extern, don't emit any code for it
			if (variable->isExtern)
			{
				break;
			}

			char *varName = thisMember->name;
			int varSize = Scope_getSizeOfType(table->globalScope, &variable->type);

			if (variable->type.initializeTo != NULL)
			{
				if (variable->isStringLiteral) // put string literals in rodata
				{
					fprintf(outFile, ".section\t.rodata\n");
				}
				else // put initialized data in sdata
				{
					fprintf(outFile, ".section\t.data\n");
				}
			}
			else // put uninitialized data to bss
			{
				fprintf(outFile, ".section\t.bss\n");
			}

			fprintf(outFile, "\t.globl %s\n", varName);

			int alignBits = -1;

			if (variable->type.arraySize > 0)
			{
				alignBits = alignSize(Scope_getSizeOfArrayElement(table->globalScope, variable));
			}
			else
			{
				alignBits = alignSize(varSize);
			}

			if (alignBits > 0)
			{
				fprintf(outFile, ".align %d\n", alignBits);
			}

			fprintf(outFile, "\t.type\t%s, @object\n", varName);
			fprintf(outFile, "\t.size \t%s, %d\n", varName, varSize);
			fprintf(outFile, "%s:\n", varName);
			if (variable->type.initializeTo != NULL)
			{
				if (variable->isStringLiteral)
				{
					int arrayElementSize = Scope_getSizeOfArrayElement(table->globalScope, variable);
					if (arrayElementSize != 1)
					{
						ErrorAndExit(ERROR_INTERNAL, "Saw array element size of %d for string literal (expected 1)!\n", arrayElementSize);
					}

					fprintf(outFile, "\t.asciz \"");
					for (int i = 0; i < varSize; i++)
					{
						for (int j = 0; j < arrayElementSize; j++)
						{
							fprintf(outFile, "%c", variable->type.initializeArrayTo[i][j]);
						}
					}
					fprintf(outFile, "\"\n");
				}
				else if (variable->type.arraySize > 0)
				{
					int arrayElementSize = Scope_getSizeOfArrayElement(table->globalScope, variable);
					for (int i = 0; i < varSize / arrayElementSize; i++)
					{
						for (int j = 0; j < arrayElementSize; j++)
						{
							fprintf(outFile, "\t.byte %d\n", variable->type.initializeArrayTo[i][j]);
						}
					}
				}
				else
				{
					for (int i = 0; i < varSize; i++)
					{
						printf("%c\n", variable->type.initializeTo[i]);
						fprintf(outFile, "\t.byte %d\n", variable->type.initializeTo[i]);
					}
				}
			}
			else
			{
				fprintf(outFile, "\t.zero %d\n", varSize);
			}

			fprintf(outFile, ".section .text\n");
		}
		break;

		default:
			break;
		}
	}
};

void calleeSaveRegisters(struct CodegenContext *context, struct CodegenMetadata *metadata)
{
	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Callee-saving touched registers\n");
	}

	// callee-save all registers (FIXME - caller vs callee save ABI?)
	int regNumSaved = 0;
	for (int i = START_ALLOCATING_FROM; i < MACHINE_REGISTER_COUNT; i++)
	{
		if (metadata->touchedRegisters[i] && (i != RETURN_REGISTER))
		{
			// store registers we modify
			EmitFrameStoreForSize(NULL,
								  context,
								  i,
								  MACHINE_REGISTER_SIZE_BYTES,
								  (-1 * (metadata->localStackSize + ((regNumSaved + 1) * MACHINE_REGISTER_SIZE_BYTES)))); // (regNumSaved + 1) to account for stack growth downwards
			regNumSaved++;
		}
	}
}

void calleeRestoreRegisters(struct CodegenContext *context, struct CodegenMetadata *metadata)
{
	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Callee-restoring touched registers\n");
	}

	// callee-save all registers (FIXME - caller vs callee save ABI?)
	int regNumRestored = 0;
	for (int i = START_ALLOCATING_FROM; i < MACHINE_REGISTER_COUNT; i++)
	{
		if (metadata->touchedRegisters[i] && (i != RETURN_REGISTER))
		{
			// store registers we modify
			EmitFrameLoadForSize(NULL,
								 context,
								 i,
								 MACHINE_REGISTER_SIZE_BYTES,
								 (-1 * (metadata->localStackSize + ((regNumRestored + 1) * MACHINE_REGISTER_SIZE_BYTES)))); // (regNumRestored + 1) to account for stack growth downwards
			regNumRestored++;
		}
	}
}

void emitPrologue(struct CodegenContext *context, struct CodegenMetadata *metadata)
{
	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Starting prologue\n");
	}

	// save return address (if necessary) and frame pointer to the stack so they will be persisted across this function
	if (metadata->function->callsOtherFunction || metadata->function->isAsmFun)
	{
		EmitStackStoreForSize(NULL,
							  context,
							  ra,
							  MACHINE_REGISTER_SIZE_BYTES,
							  (-1 * (metadata->localStackSize + metadata->calleeSaveStackSize - ((1) * MACHINE_REGISTER_SIZE_BYTES))));
	}
	EmitStackStoreForSize(NULL,
						  context,
						  fp,
						  MACHINE_REGISTER_SIZE_BYTES,
						  (-1 * (metadata->localStackSize + metadata->calleeSaveStackSize - ((0) * MACHINE_REGISTER_SIZE_BYTES))));

	emitInstruction(NULL, context, "\tmv %s, %s\n", registerNames[fp], registerNames[sp]); // generate new fp

	if (metadata->totalStackSize)
	{
		emitInstruction(NULL, context, "\t# %d bytes locals, %d bytes callee-save\n", metadata->localStackSize, metadata->calleeSaveStackSize);
		emitInstruction(NULL, context, "\taddi %s, %s, -%d\n", registerNames[sp], registerNames[sp], metadata->totalStackSize);
		calleeSaveRegisters(context, metadata);
	}

	// FIXME: cfa offset if no local stack?
	fprintf(context->outFile, "\t.cfi_def_cfa_offset %d\n", metadata->totalStackSize);

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Placing arguments into registers\n");
	}

	// move any applicable arguments into registers if we are expecting them not to be spilled
	for (struct LinkedListNode *ltRunner = metadata->allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
	{
		struct Lifetime *thisLifetime = ltRunner->data;

		// (short-circuit away from looking up temps since they can't be arguments)
		if (thisLifetime->name[0] != '.')
		{
			struct ScopeMember *thisEntry = Scope_lookup(metadata->function->mainScope, thisLifetime->name);
			// we need to place this variable into its register if:
			if ((thisEntry != NULL) &&							 // it exists
				(thisEntry->type == e_argument) &&				 // it's an argument
				(thisLifetime->wbLocation == wb_register) &&	 // it lives in a register
				(thisLifetime->nreads || thisLifetime->nwrites)) // theyre are either read from or written to at all
			{
				struct VariableEntry *theArgument = thisEntry->entry;

				char loadWidth = SelectWidthCharForLifetime(metadata->function->mainScope, thisLifetime);
				emitInstruction(NULL, context, "\tl%c%s %s, %d(fp) # place %s\n",
								loadWidth,
								SelectSignForLoad(loadWidth, &thisLifetime->type),
								registerNames[thisLifetime->registerLocation],
								theArgument->stackOffset,
								thisLifetime->name);
			}
		}
	}
}

void emitEpilogue(struct CodegenContext *context, struct CodegenMetadata *metadata)
{
	fprintf(context->outFile, "%s_done:\n", metadata->function->name);

	calleeRestoreRegisters(context, metadata);

	// load saved return address (if necessary) and saved frame pointer to the stack so they will be persisted across this function

	if (metadata->function->callsOtherFunction || metadata->function->isAsmFun)
	{
		EmitFrameLoadForSize(NULL,
							 context,
							 ra,
							 MACHINE_REGISTER_SIZE_BYTES,
							 (-1 * (metadata->localStackSize + metadata->calleeSaveStackSize - ((1) * MACHINE_REGISTER_SIZE_BYTES))));
	}

	EmitFrameLoadForSize(NULL,
						 context,
						 fp,
						 MACHINE_REGISTER_SIZE_BYTES,
						 (-1 * (metadata->localStackSize + metadata->calleeSaveStackSize - ((0) * MACHINE_REGISTER_SIZE_BYTES))));

	int localAndArgStackSize = metadata->totalStackSize + metadata->function->argStackSize;
	if (localAndArgStackSize > 0)
	{
		emitInstruction(NULL, context, "\t# %d bytes locals, %d bytes callee-save, %d bytes arguments\n", metadata->totalStackSize - (MACHINE_REGISTER_SIZE_BYTES * metadata->nRegistersCalleeSaved), (MACHINE_REGISTER_SIZE_BYTES * metadata->nRegistersCalleeSaved), metadata->function->argStackSize);
		emitInstruction(NULL, context, "\taddi %s, %s, %d\n", registerNames[sp], registerNames[sp], localAndArgStackSize);
	}
	// FIXME: cfa offset if no local stack?

	fprintf(context->outFile, "\t.cfi_def_cfa_offset %d\n", metadata->totalStackSize + (2 * MACHINE_REGISTER_SIZE_BYTES));

	fprintf(context->outFile, "\t.cfi_def_cfa_offset 0\n");
	emitInstruction(NULL, context, "\tjalr zero, 0(%s)\n", registerNames[ra]);

	fprintf(context->outFile, "\t.cfi_endproc\n");
}

/*
 * code generation for funcitons (lifetime management, etc)
 *
 */
extern struct Config config;
void generateCodeForFunction(FILE *outFile, struct FunctionEntry *function)
{
	currentVerbosity = config.stageVerbosities[STAGE_CODEGEN];
	int instructionIndex = 0; // index from start of function in terms of number of instructions
	struct CodegenContext context;
	context.outFile = outFile;
	context.instructionIndex = &instructionIndex;

	if (currentVerbosity > VERBOSITY_SILENT)
	{
		printf("Generate code for function %s\n", function->name);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Emitting function prologue\n");
	}
	fprintf(outFile, ".align 2\n%s:\n", function->name);
	fprintf(outFile, "\t.loc 1 %d %d\n", function->correspondingTree.sourceLine, function->correspondingTree.sourceCol);
	fprintf(outFile, "\t.cfi_startproc\n");

	struct CodegenMetadata metadata;
	memset(&metadata, 0, sizeof(struct CodegenMetadata));
	metadata.function = function;
	metadata.reservedRegisters[0] = -1;
	metadata.reservedRegisters[1] = -1;
	metadata.reservedRegisters[2] = -1;
	metadata.reservedRegisterCount = 0;
	currentVerbosity = config.stageVerbosities[STAGE_REGALLOC];
	allocateRegisters(&metadata);
	currentVerbosity = config.stageVerbosities[STAGE_CODEGEN];

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Need %d bytes on stack\n", metadata.totalStackSize);
	}

	emitPrologue(&context, &metadata);

	// TODO: debug symbols for asm functions?
	if ((currentVerbosity > VERBOSITY_MINIMAL) && (function->isAsmFun))
	{
		printf("%s is an asm function\n", function->name);
	}

	if (function->isAsmFun && (function->BasicBlockList->size != 1))
	{
		ErrorAndExit(ERROR_INTERNAL, "Asm function with %d basic blocks seen - expected 1!\n", function->BasicBlockList->size);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Generating code for basic blocks\n");
	}

	for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
	{
		struct BasicBlock *block = blockRunner->data;
		generateCodeForBasicBlock(&context, block, function->mainScope, metadata.allLifetimes, function->name, metadata.reservedRegisters);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Emitting function epilogue\n");
	}

	emitEpilogue(&context, &metadata);

	// clean up after ourselves

	LinkedList_Free(metadata.allLifetimes, free);

	for (int i = 0; i <= metadata.largestTacIndex; i++)
	{
		LinkedList_Free(metadata.lifetimeOverlaps[i], NULL);
	}
	free(metadata.lifetimeOverlaps);
}

void generateCodeForBasicBlock(struct CodegenContext *context,
							   struct BasicBlock *block,
							   struct Scope *scope,
							   struct LinkedList *lifetimes,
							   char *functionName,
							   int reservedRegisters[3])
{
	// we may pass null if we are generating the code to initialize global variables
	if (functionName != NULL)
	{
		fprintf(context->outFile, "%s_%d:\n", functionName, block->labelNum);
	}

	int lastLineNo = -1;
	for (struct LinkedListNode *TACRunner = block->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
	{
		struct TACLine *thisTAC = TACRunner->data;

		char *printedTAC = sPrintTACLine(thisTAC);
		fprintf(context->outFile, "#%s\n", printedTAC);
		free(printedTAC);

		// don't duplicate .loc's for the same line
		// riscv64-unknown-elf-gdb (or maybe the as/ld) don't enjoy going backwards/staying put in line or column loc
		if (thisTAC->correspondingTree.sourceLine > lastLineNo)
		{
			fprintf(context->outFile, "\t.loc 1 %d\n", thisTAC->correspondingTree.sourceLine);
			lastLineNo = thisTAC->correspondingTree.sourceLine;
		}

		switch (thisTAC->operation)
		{
		case tt_asm:
			fputs(thisTAC->operands[0].name.str, context->outFile);
			fputc('\n', context->outFile);
			break;

		case tt_assign:
		{
			// only works for primitive types that will fit in registers
			int opLocReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], opLocReg);
		}
		break;

		case tt_add:
		case tt_subtract:
		case tt_mul:
		case tt_div:
		case tt_modulo:
		case tt_bitwise_and:
		case tt_bitwise_or:
		case tt_bitwise_xor:
		case tt_lshift:
		case tt_rshift:
		{
			int op1Reg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int op2Reg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);

			emitInstruction(thisTAC, context, "\t%s %s, %s, %s\n", getAsmOp(thisTAC->operation), registerNames[destReg], registerNames[op1Reg], registerNames[op2Reg]);
			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_bitwise_not:
		{
			int op1Reg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);

			emitInstruction(thisTAC, context, "\txori %s, %s, -1\n", registerNames[destReg], registerNames[op1Reg]);
			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_load:
		{
			int baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[1]);

			char loadWidth = SelectWidthChar(scope, &thisTAC->operands[0]);
			emitInstruction(thisTAC, context, "\tl%c%s %s, 0(%s)\n",
							loadWidth,
							SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 0)),
							registerNames[destReg],
							registerNames[baseReg]);

			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_load_off:
		{
			// TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);

			char loadWidth = SelectWidthChar(scope, &thisTAC->operands[0]);
			emitInstruction(thisTAC, context, "\tl%c%s %s, %d(%s)\n",
							loadWidth,
							SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 0)),
							registerNames[destReg],
							thisTAC->operands[2].name.val,
							registerNames[baseReg]);

			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_load_arr:
		{
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			int offsetReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[2]);

			// TODO: check for shift by 0 and don't shift when applicable
			// perform a left shift by however many bits necessary to scale our value, place the result in reservedRegisters[2]
			emitInstruction(thisTAC, context, "\tslli %s, %s, %d\n",
							registerNames[reservedRegisters[2]],
							registerNames[offsetReg],
							thisTAC->operands[3].name.val);

			// add our scaled offset to the base address, put the full address into reservedRegisters[1]
			emitInstruction(thisTAC, context, "\tadd %s, %s, %s\n",
							registerNames[reservedRegisters[1]],
							registerNames[baseReg],
							registerNames[reservedRegisters[2]]);

			char loadWidth = SelectWidthCharForDereference(scope, &thisTAC->operands[1]);
			emitInstruction(thisTAC, context, "\tl%c%s %s, 0(%s)\n",
							loadWidth,
							SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 1)),
							registerNames[destReg],
							registerNames[reservedRegisters[1]]);

			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_store:
		{
			int destAddrReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			char storeWidth = SelectWidthCharForDereference(scope, &thisTAC->operands[0]);

			emitInstruction(thisTAC, context, "\ts%c %s, 0(%s)\n",
							storeWidth,
							registerNames[sourceReg],
							registerNames[destAddrReg]);
		}
		break;

		case tt_store_off:
		{
			// TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
			int baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			emitInstruction(thisTAC, context, "\ts%c %s, %d(%s)\n",
							SelectWidthChar(scope, &thisTAC->operands[0]),
							registerNames[sourceReg],
							thisTAC->operands[1].name.val,
							registerNames[baseReg]);
		}
		break;

		case tt_store_arr:
		{
			int baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int offsetReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			int sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[3], reservedRegisters[2]);

			// TODO: check for shift by 0 and don't shift when applicable
			// perform a left shift by however many bits necessary to scale our value, place the result in reservedRegisters[1]
			emitInstruction(thisTAC, context, "\tslli %s, %s, %d\n",
							registerNames[reservedRegisters[1]],
							registerNames[offsetReg],
							thisTAC->operands[2].name.val);

			// add our scaled offset to the base address, put the full address into reservedRegisters[0]
			emitInstruction(thisTAC, context, "\tadd %s, %s, %s\n",
							registerNames[reservedRegisters[0]],
							registerNames[baseReg],
							registerNames[reservedRegisters[1]]);

			emitInstruction(thisTAC, context, "\ts%c %s, 0(%s)\n",
							SelectWidthCharForDereference(scope, &thisTAC->operands[0]),
							registerNames[sourceReg],
							registerNames[reservedRegisters[0]]);
		}
		break;

		case tt_addrof:
		{
			int addrReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			addrReg = placeAddrOfLifetimeInReg(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], addrReg);
			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], addrReg);
		}
		break;

		case tt_lea_off:
		{
			// TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);

			emitInstruction(thisTAC, context, "\taddi %s, %s, %d\n",
							registerNames[destReg],
							registerNames[baseReg],
							thisTAC->operands[2].name.val);

			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_lea_arr:
		{
			int destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			int offsetReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[2]);

			// TODO: check for shift by 0 and don't shift when applicable
			// perform a left shift by however many bits necessary to scale our value, place the result in reservedRegisters[1]
			emitInstruction(thisTAC, context, "\tslli %s, %s, %d\n",
							registerNames[reservedRegisters[2]],
							registerNames[offsetReg],
							thisTAC->operands[3].name.val);

			// add our scaled offset to the base address, put the full address into destReg
			emitInstruction(thisTAC, context, "\tadd %s, %s, %s\n",
							registerNames[destReg],
							registerNames[baseReg],
							registerNames[reservedRegisters[2]]);

			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
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
			int operand1register = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
			int operand2register = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
			emitInstruction(thisTAC, context, "\t%s %s, %s, %s_%d\n",
							getAsmOp(thisTAC->operation),
							registerNames[operand1register],
							registerNames[operand2register],
							functionName,
							thisTAC->operands[0].name.val);
		}
		break;

		case tt_jmp:
		{
			emitInstruction(thisTAC, context, "\tj %s_%d\n", functionName, thisTAC->operands[0].name.val);
		}
		break;

		case tt_stack_reserve:
		{
			emitInstruction(thisTAC, context, "\taddi %s, %s, -%d\n", registerNames[sp], registerNames[sp], thisTAC->operands[0].name.val);
		}
		break;

		case tt_stack_store:
		{
			int sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);

			EmitStackStoreForSize(thisTAC,
								  context,
								  sourceReg,
								  Scope_getSizeOfType(scope, TAC_GetTypeOfOperand(thisTAC, 0)),
								  thisTAC->operands[1].name.val);
		}
		break;

		case tt_call:
		{
			struct FunctionEntry *called = Scope_lookupFunByString(scope, thisTAC->operands[1].name.str);
			if (called->isDefined)
			{
				emitInstruction(thisTAC, context, "\tcall %s\n", thisTAC->operands[1].name.str);
			}
			else
			{
				emitInstruction(thisTAC, context, "\tcall %s@plt\n", thisTAC->operands[1].name.str);
			}

			if (thisTAC->operands[0].name.str != NULL)
			{
				WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], RETURN_REGISTER);
			}
		}
		break;

		case tt_label:
			fprintf(context->outFile, "\t%s_%ld:\n", functionName, thisTAC->operands[0].name.val);
			break;

		case tt_return:
		{
			if (thisTAC->operands[0].name.str != NULL)
			{
				int sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], RETURN_REGISTER);

				if (sourceReg != RETURN_REGISTER)
				{
					emitInstruction(thisTAC, context, "\tmv %s, %s\n",
									registerNames[RETURN_REGISTER],
									registerNames[sourceReg]);
				}
			}
			emitInstruction(thisTAC, context, "\tj %s_done\n", scope->parentFunction->name);
		}
		break;

		case tt_do:
		case tt_enddo:
			break;
		}
	}
}
