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
			struct VariableEntry *v = thisMember->entry;

			// early break if the variable is declared as extern, don't emit any code for it
			if (v->isExtern)
			{
				break;
			}

			char *varName = thisMember->name;
			int varSize = Scope_getSizeOfType(table->globalScope, &v->type);

			if (v->type.initializeTo != NULL)
			{
				if (v->isStringLiteral) // put string literals in rodata
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

			fprintf(outFile, "\t.align %d\n", alignSize(varSize));
			fprintf(outFile, "\t.type\t%s, @object\n", varName);
			fprintf(outFile, "\t.size \t%s, %d\n", varName, varSize);
			fprintf(outFile, "%s:\n", varName);
			if (v->type.initializeTo != NULL)
			{
				if (v->isStringLiteral)
				{
					int arrayElementSize = Scope_getSizeOfArrayElement(table->globalScope, v);
					if (arrayElementSize != 1)
					{
						ErrorAndExit(ERROR_INTERNAL, "Saw array element size of %d for string literal (expected 1)!\n", arrayElementSize);
					}

					fprintf(outFile, "\t.asciz \"");
					for (int i = 0; i < varSize; i++)
					{
						for (int j = 0; j < arrayElementSize; j++)
						{
							fprintf(outFile, "%c", v->type.initializeArrayTo[i][j]);
						}
					}
					fprintf(outFile, "\"\n");
				}
				else if (v->type.arraySize > 0)
				{
					int arrayElementSize = Scope_getSizeOfArrayElement(table->globalScope, v);
					for (int i = 0; i < varSize / arrayElementSize; i++)
					{
						for (int j = 0; j < arrayElementSize; j++)
						{
							fprintf(outFile, "\t.byte %d\n", v->type.initializeArrayTo[i][j]);
						}
					}
				}
				else
				{
					for (int i = 0; i < varSize; i++)
					{
						printf("%c\n", v->type.initializeTo[i]);
						fprintf(outFile, "\t.byte %d\n", v->type.initializeTo[i]);
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
	fprintf(outFile, "%s:\n", function->name);
	fprintf(outFile, "\t.loc 1 %d %d\n", function->correspondingTree.sourceLine, function->correspondingTree.sourceCol);
	fprintf(outFile, "\t.cfi_startproc\n");

	// push return address
	EmitPushForSize(NULL, &context, 4, 1);
	fprintf(outFile, "\t.cfi_offset 1, -4\n");

	// push frame pointer, copy stack pointer to frame pointer
	EmitPushForSize(NULL, &context, 4, 8);
	fprintf(outFile, "\t.cfi_offset 8, -8\n");
	emitInstruction(NULL, &context, "\tmv fp, sp\n");

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
				ErrorWithAST(ERROR_INTERNAL, &asmTAC->correspondingTree, "Non-asm TAC type seen in asm function!\n");
			}
			emitInstruction(asmTAC, &context, "\t%s\n", asmTAC->operands[0].name.str);
		}

		// pop frame pointer
		EmitPopForSize(NULL, &context, 4, 8);
		fprintf(outFile, "\t.cfi_restore 8\n");

		// pop return address
		EmitPopForSize(NULL, &context, 4, 1);
		fprintf(outFile, "\t.cfi_restore 1\n");

		emitInstruction(NULL, &context, "\taddi sp, sp, %d\n", function->argStackSize);

		fprintf(outFile, "\t.cfi_def_cfa_offset 0\n");
		emitInstruction(NULL, &context, "\tjalr zero, 0(%s)\n", registerNames[1]);
		fprintf(outFile, "\t.cfi_endproc\n");

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
	int localStackSize = allocateRegisters(&metadata);
	currentVerbosity = config.stageVerbosities[STAGE_CODEGEN];

	if (localStackSize > 0)
	{
		emitInstruction(NULL, &context, "\taddi sp, sp, -%d\n", localStackSize);
		fprintf(outFile, "\t.cfi_def_cfa_offset %d\n", localStackSize + 8);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Need %d bytes on stack\n", localStackSize);
	}
	for (int i = MACHINE_REGISTER_COUNT - 1; i >= 0; i--)
	{
		if (metadata.touchedRegisters[i] && (i != RETURN_REGISTER))
		{
			EmitPushForSize(NULL, &context, 4, i);
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
				emitInstruction(NULL, &context, "\tl%su %s, %d(fp) # place %s\n",
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
		generateCodeForBasicBlock(&context, block, function->mainScope, metadata.allLifetimes, function->name, metadata.reservedRegisters);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Emitting function epilogue\n");
	}

	fprintf(outFile, "%s_done:\n", function->name);
	for (int i = 0; i < MACHINE_REGISTER_COUNT; i++)
	{
		if (metadata.touchedRegisters[i] && (i != RETURN_REGISTER))
		{
			EmitPopForSize(NULL, &context, 4, i);
		}
	}

	if (localStackSize > 0)
	{
		emitInstruction(NULL, &context, "\taddi sp, sp, %d\n", localStackSize);
	}

	// pop frame pointer
	EmitPopForSize(NULL, &context, 4, 8);
	fprintf(outFile, "\t.cfi_restore 8\n");

	// pop return address
	EmitPopForSize(NULL, &context, 4, 1);
	fprintf(outFile, "\t.cfi_restore 1\n");

	if (function->argStackSize > 0)
	{
		emitInstruction(NULL, &context, "\taddi sp, sp, %d\n", function->argStackSize);
	}

	fprintf(outFile, "\t.cfi_def_cfa_offset 0\n");
	emitInstruction(NULL, &context, "\tjalr zero, 0(%s)\n", registerNames[1]);

	fprintf(outFile, "\t.cfi_endproc\n");

	// function setup and teardown code generated

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

			const char *loadWidth = SelectWidth(scope, &thisTAC->operands[0]);
			emitInstruction(thisTAC, context, "\tl%su %s, 0(%s)\n",
							loadWidth,
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

			emitInstruction(thisTAC, context, "\tl%su %s, %d(%s)\n",
							SelectWidth(scope, &thisTAC->operands[0]),
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

			emitInstruction(thisTAC, context, "\tl%su %s, 0(%s)\n",
							SelectWidthForDereference(scope, &thisTAC->operands[1]),
							registerNames[destReg],
							registerNames[reservedRegisters[1]]);

			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
		}
		break;

		case tt_store:
		{
			int destAddrReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			int sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
			const char *storeWidth = SelectWidthForDereference(scope, &thisTAC->operands[0]);

			emitInstruction(thisTAC, context, "\ts%s %s, 0(%s)\n",
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
			emitInstruction(thisTAC, context, "\ts%s %s, %d(%s)\n",
							SelectWidth(scope, &thisTAC->operands[0]),
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

			emitInstruction(thisTAC, context, "\ts%s %s, 0(%s)\n",
							SelectWidthForDereference(scope, &thisTAC->operands[0]),
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

		case tt_push:
		{
			int operandRegister = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			EmitPushForOperand(thisTAC, context, scope, &thisTAC->operands[0], operandRegister);
		}
		break;

		case tt_pop:
		{
			int operandRegister = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
			EmitPopForOperand(thisTAC, context, scope, &thisTAC->operands[0], operandRegister);
			WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], operandRegister);
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
			fprintf(context->outFile, "\t%s_%d:\n", functionName, thisTAC->operands[0].name.val);
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
