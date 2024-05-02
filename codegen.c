#include "codegen.h"

#include "codegen_generic.h"
#include "log.h"
#include "regalloc.h"
#include "symtab.h"

void generateCodeForProgram(struct SymbolTable *table, FILE *outFile)
{
    struct CodegenContext globalContext;
    size_t globalInstructionIndex = 0;
    globalContext.instructionIndex = &globalInstructionIndex;
    globalContext.outFile = outFile;

    // fprintf(outFile, "\t.text\n");
    for (size_t entryIndex = 0; entryIndex < table->globalScope->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = table->globalScope->entries->data[entryIndex];
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

            generateCodeForFunction(outFile, generatedFunction, NULL);
            fprintf(outFile, "\t.size %s, .-%s\n", generatedFunction->name, generatedFunction->name);
        }
        break;

        case e_basicblock:
        {
            generateCodeForGlobalBlock(&globalContext, table->globalScope, thisMember->entry);
        }
        break;

        case e_variable:
        {
            generateCodeForGlobalVariable(&globalContext, table->globalScope, thisMember->entry);
        }
        break;

        case e_class:
        {
            generateCodeForClass(&globalContext, thisMember->entry);
        }
        break;

        default:
            break;
        }
    }
};

void generateCodeForClass(struct CodegenContext *globalContext, struct ClassEntry *class)
{
    for (size_t entryIndex = 0; entryIndex < class->members->entries->size; entryIndex++)
    {
        struct ScopeMember *thisMember = class->members->entries->data[entryIndex];
        switch (thisMember->type)
        {
        case e_function:
        {
            generateCodeForFunction(globalContext->outFile, thisMember->entry, class->name);
        }
        break;

        default:
            break;
        }
    }
}

void generateCodeForGlobalBlock(struct CodegenContext *globalContext, struct Scope *globalScope, struct BasicBlock *globalBlock)
{
    // early return if no code to generate
    if (globalBlock->TACList->size == 0)
    {
        return;
    }
    // compiled code
    if (globalBlock->labelNum == 0)
    {
        fprintf(globalContext->outFile, ".userstart:\n");
        struct LinkedList *fakeBlockList = LinkedList_New();
        LinkedList_Append(fakeBlockList, globalBlock);

        struct LinkedList *globalLifetimes = findLifetimes(globalScope, fakeBlockList);
        LinkedList_Free(fakeBlockList, NULL);
        // TODO: defines for default reserved registers? This is for global-scoped code... 0 1 and 2 are definitely not right.
        u8 reserved[3] = {0, 1, 2};

        generateCodeForBasicBlock(globalContext, globalBlock, globalScope, globalLifetimes, NULL, reserved);
        LinkedList_Free(globalLifetimes, free);
    } // assembly block
    else if (globalBlock->labelNum == 1)
    {

        fprintf(globalContext->outFile, ".rawasm:\n");

        for (struct LinkedListNode *examinedLine = globalBlock->TACList->head; examinedLine != NULL; examinedLine = examinedLine->next)
        {
            struct TACLine *examinedTAC = examinedLine->data;
            if (examinedTAC->operation != tt_asm)
            {
                InternalError("Unexpected TAC type %d (%s) seen in global ASM block!\n",
                              examinedTAC->operation,
                              getAsmOp(examinedTAC->operation));
            }
            fprintf(globalContext->outFile, "%s\n", examinedTAC->operands[0].name.str);
        }
    }
    else
    {
        InternalError("Unexpected basic block index %zu at global scope!", globalBlock->labelNum);
    }
}

void generateCodeForObject(struct CodegenContext *globalContext, struct Scope *globalScope, struct Type *type)
{
    // how to handle multidimensional arrays with intiializeArrayTo at each level? Nested labels for nested elements?
    if (type->basicType == vt_array)
    {
        InternalError("generateCodeForObject called with array type - not supported yet!\n");
    }
    else
    {
        if (type->nonArray.initializeTo != NULL)
        {
            size_t objectSize = Type_GetSize(type, globalScope);
            for (size_t byteIndex = 0; byteIndex < objectSize; byteIndex++)
            {
                fprintf(globalContext->outFile, "\t.byte %d\n", (type->nonArray.initializeTo)[byteIndex]);
            }
        }
        else
        {
            fprintf(globalContext->outFile, "\t.zero %zu\n", Type_GetSize(type, globalScope));
        }
    }
}

void generateCodeForGlobalVariable(struct CodegenContext *globalContext, struct Scope *globalScope, struct VariableEntry *variable)
{
    // early return if the variable is declared as extern, don't emit any code for it
    if (variable->isExtern)
    {
        return;
    }

    char *varName = variable->name;
    size_t varSize = Type_GetSize(&variable->type, globalScope);

    if (variable->type.basicType == vt_array)
    {
        // string literals go in rodata
        if ((variable->type.array.initializeArrayTo != NULL) && (variable->isStringLiteral))
        {
            fprintf(globalContext->outFile, ".section\t.rodata\n");
        }
        fprintf(globalContext->outFile, ".section\t.data\n");
    }
    else
    {
        fprintf(globalContext->outFile, ".section\t.bss\n");
    }

    fprintf(globalContext->outFile, "\t.globl %s\n", varName);

    u8 alignBits = Type_GetAlignment(&variable->type, globalScope);
    if (alignBits > 0)
    {
        fprintf(globalContext->outFile, ".align %d\n", alignBits);
    }

    fprintf(globalContext->outFile, "\t.type\t%s, @object\n", varName);
    fprintf(globalContext->outFile, "\t.size \t%s, %zu\n", varName, varSize);
    fprintf(globalContext->outFile, "%s:\n", varName);

    if (variable->type.basicType == vt_array)
    {
        if (variable->type.array.initializeArrayTo != NULL)
        {
            if (variable->isStringLiteral)
            {
                fprintf(globalContext->outFile, "\t.asciz \"");
                for (size_t charIndex = 0; charIndex < variable->type.array.size; charIndex++)
                {
                    fprintf(globalContext->outFile, "%c", ((char *)variable->type.array.initializeArrayTo[charIndex])[0]);
                }
                fprintf(globalContext->outFile, "\"\n");
            }
            else
            {
                generateCodeForObject(globalContext, globalScope, &variable->type);
            }
        }
    }
    else
    {
        generateCodeForObject(globalContext, globalScope, &variable->type);
    }

    fprintf(globalContext->outFile, ".section .text\n");
}

void calleeSaveRegisters(struct CodegenContext *context, struct CodegenMetadata *metadata)
{
    Log(LOG_DEBUG, "Callee-saving touched registers");

    // callee-save all registers (FIXME - caller vs callee save ABI?)
    u8 regNumSaved = 0;
    for (u8 reg = START_ALLOCATING_FROM; reg < MACHINE_REGISTER_COUNT; reg++)
    {
        if (metadata->touchedRegisters[reg] && (reg != RETURN_REGISTER))
        {
            // store registers we modify
            EmitFrameStoreForSize(NULL,
                                  context,
                                  reg,
                                  MACHINE_REGISTER_SIZE_BYTES,
                                  (-1 * (ssize_t)(metadata->localStackSize + ((regNumSaved + 1) * MACHINE_REGISTER_SIZE_BYTES)))); // (regNumSaved + 1) to account for stack growth downwards
            regNumSaved++;
        }
    }
}

void calleeRestoreRegisters(struct CodegenContext *context, struct CodegenMetadata *metadata)
{
    Log(LOG_DEBUG, "Callee-restoring touched registers");

    // callee-save all registers (FIXME - caller vs callee save ABI?)
    u8 regNumRestored = 0;
    for (u8 reg = START_ALLOCATING_FROM; reg < MACHINE_REGISTER_COUNT; reg++)
    {
        if (metadata->touchedRegisters[reg] && (reg != RETURN_REGISTER))
        {
            // store registers we modify
            EmitFrameLoadForSize(NULL,
                                 context,
                                 reg,
                                 MACHINE_REGISTER_SIZE_BYTES,
                                 (-1 * (ssize_t)(metadata->localStackSize + ((regNumRestored + 1) * MACHINE_REGISTER_SIZE_BYTES)))); // (regNumRestored + 1) to account for stack growth downwards
            regNumRestored++;
        }
    }
}

void emitPrologue(struct CodegenContext *context, struct CodegenMetadata *metadata)
{
    Log(LOG_DEBUG, "Emitting function prologue for %s", metadata->function->name);

    // save return address (if necessary) and frame pointer to the stack so they will be persisted across this function
    if (metadata->function->callsOtherFunction || metadata->function->isAsmFun)
    {
        // TODO: asserts against crazy large stack sizes causing overflow with ssize_t?
        EmitStackStoreForSize(NULL,
                              context,
                              ra,
                              MACHINE_REGISTER_SIZE_BYTES,
                              (-1 * (ssize_t)(metadata->localStackSize + metadata->calleeSaveStackSize - ((1) * MACHINE_REGISTER_SIZE_BYTES))));
    }
    EmitStackStoreForSize(NULL,
                          context,
                          fp,
                          MACHINE_REGISTER_SIZE_BYTES,
                          (-1 * (ssize_t)(metadata->localStackSize + metadata->calleeSaveStackSize - ((0) * MACHINE_REGISTER_SIZE_BYTES))));

    emitInstruction(NULL, context, "\tmv %s, %s\n", registerNames[fp], registerNames[sp]); // generate new fp

    if (metadata->totalStackSize)
    {
        emitInstruction(NULL, context, "\t# %d bytes locals, %d bytes callee-save\n", metadata->localStackSize, metadata->calleeSaveStackSize);
        emitInstruction(NULL, context, "\taddi %s, %s, -%d\n", registerNames[sp], registerNames[sp], metadata->totalStackSize);
        calleeSaveRegisters(context, metadata);
    }

    // FIXME: cfa offset if no local stack?
    fprintf(context->outFile, "\t.cfi_def_cfa_offset %zu\n", metadata->totalStackSize);

    Log(LOG_DEBUG, "Place arguments into registers");

    // move any applicable arguments into registers if we are expecting them not to be spilled
    for (struct LinkedListNode *ltRunner = metadata->allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
    {
        struct Lifetime *thisLifetime = ltRunner->data;

        // (short-circuit away from looking up temps since they can't be arguments)
        if (thisLifetime->name[0] != '.')
        {
            struct ScopeMember *thisEntry = Scope_lookup(metadata->function->mainScope, thisLifetime->name);
            // we need to place this variable into its register if:
            if ((thisEntry != NULL) &&                           // it exists
                (thisEntry->type == e_argument) &&               // it's an argument
                (thisLifetime->wbLocation == wb_register) &&     // it lives in a register
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
    Log(LOG_DEBUG, "Emit function epilogue for %s", metadata->function->name);

    fprintf(context->outFile, "%s_done:\n", metadata->function->name);

    calleeRestoreRegisters(context, metadata);

    // load saved return address (if necessary) and saved frame pointer to the stack so they will be persisted across this function

    if (metadata->function->callsOtherFunction || metadata->function->isAsmFun)
    {
        EmitFrameLoadForSize(NULL,
                             context,
                             ra,
                             MACHINE_REGISTER_SIZE_BYTES,
                             (-1 * (ssize_t)(metadata->localStackSize + metadata->calleeSaveStackSize - ((1) * MACHINE_REGISTER_SIZE_BYTES))));
    }

    EmitFrameLoadForSize(NULL,
                         context,
                         fp,
                         MACHINE_REGISTER_SIZE_BYTES,
                         (-1 * (ssize_t)(metadata->localStackSize + metadata->calleeSaveStackSize - ((0) * MACHINE_REGISTER_SIZE_BYTES))));

    size_t localAndArgStackSize = metadata->totalStackSize + metadata->function->argStackSize;
    if (localAndArgStackSize > 0)
    {
        emitInstruction(NULL, context, "\t# %d bytes locals, %d bytes callee-save, %d bytes arguments\n", metadata->totalStackSize - (MACHINE_REGISTER_SIZE_BYTES * metadata->nRegistersCalleeSaved), (MACHINE_REGISTER_SIZE_BYTES * metadata->nRegistersCalleeSaved), metadata->function->argStackSize);
        emitInstruction(NULL, context, "\taddi %s, %s, %d\n", registerNames[sp], registerNames[sp], localAndArgStackSize);
    }
    // FIXME: cfa offset if no local stack?

    fprintf(context->outFile, "\t.cfi_def_cfa_offset %zu\n", metadata->totalStackSize + (2 * MACHINE_REGISTER_SIZE_BYTES));

    fprintf(context->outFile, "\t.cfi_def_cfa_offset 0\n");
    emitInstruction(NULL, context, "\tjalr zero, 0(%s)\n", registerNames[ra]);

    fprintf(context->outFile, "\t.cfi_endproc\n");
}

/*
 * code generation for funcitons (lifetime management, etc)
 *
 */
extern struct Config config;
void generateCodeForFunction(FILE *outFile, struct FunctionEntry *function, char *methodOfClassName)
{
    char *fullFunctionName = function->name;
    if (methodOfClassName != NULL)
    {
        // TODO: member function name mangling/uniqueness
        fullFunctionName = malloc(strlen(function->name) + strlen(methodOfClassName) + 2);
        strcpy(fullFunctionName, methodOfClassName);
        strcat(fullFunctionName, "_");
        strcat(fullFunctionName, function->name);
    }
    size_t instructionIndex = 0; // index from start of function in terms of number of instructions
    struct CodegenContext context;
    context.outFile = outFile;
    context.instructionIndex = &instructionIndex;

    Log(LOG_INFO, "Generate code for function %s", fullFunctionName);

    fprintf(outFile, ".align 2\n%s:\n", fullFunctionName);
    fprintf(outFile, "\t.loc 1 %d %d\n", function->correspondingTree.sourceLine, function->correspondingTree.sourceCol);
    fprintf(outFile, "\t.cfi_startproc\n");

    struct CodegenMetadata metadata;
    memset(&metadata, 0, sizeof(struct CodegenMetadata));
    metadata.function = function;
    metadata.reservedRegisters[0] = -1;
    metadata.reservedRegisters[1] = -1;
    metadata.reservedRegisters[2] = -1;
    metadata.reservedRegisterCount = 0;
    allocateRegisters(&metadata);

    // TODO: debug symbols for asm functions?
    if (function->isAsmFun)
    {
        Log(LOG_DEBUG, "%s is an asm function", function->name);
    }

    emitPrologue(&context, &metadata);

    if (function->isAsmFun && (function->BasicBlockList->size != 1))
    {
        InternalError("Asm function with %zu basic blocks seen - expected 1!", function->BasicBlockList->size);
    }

    for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
    {
        struct BasicBlock *block = blockRunner->data;
        Log(LOG_DEBUG, "Generating code for basic block %zd", block->labelNum);
        generateCodeForBasicBlock(&context, block, function->mainScope, metadata.allLifetimes, fullFunctionName, metadata.reservedRegisters);
    }

    emitEpilogue(&context, &metadata);

    // clean up after ourselves
    LinkedList_Free(metadata.allLifetimes, free);

    for (size_t tacIndex = 0; tacIndex <= metadata.largestTacIndex; tacIndex++)
    {
        LinkedList_Free(metadata.lifetimeOverlaps[tacIndex], NULL);
    }
    free(metadata.lifetimeOverlaps);

    if (methodOfClassName != NULL)
    {
        free(fullFunctionName);
    }
}

void generateCodeForBasicBlock(struct CodegenContext *context,
                               struct BasicBlock *block,
                               struct Scope *scope,
                               struct LinkedList *lifetimes,
                               char *functionName,
                               u8 reservedRegisters[3])
{
    // we may pass null if we are generating the code to initialize global variables
    if (functionName != NULL)
    {
        fprintf(context->outFile, "%s_%zu:\n", functionName, block->labelNum);
    }

    u32 lastLineNo = 0;
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
            u8 opLocReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
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
            u8 op1Reg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
            u8 op2Reg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
            u8 destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);

            emitInstruction(thisTAC, context, "\t%s %s, %s, %s\n", getAsmOp(thisTAC->operation), registerNames[destReg], registerNames[op1Reg], registerNames[op2Reg]);
            WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_bitwise_not:
        {
            u8 op1Reg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
            u8 destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);

            emitInstruction(thisTAC, context, "\txori %s, %s, -1\n", registerNames[destReg], registerNames[op1Reg]);
            WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_load:
        {
            u8 baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
            u8 destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[1]);

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
            u8 destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
            u8 baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);

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
            u8 destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
            u8 baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
            u8 offsetReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[2]);

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
            u8 destAddrReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
            u8 sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
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
            u8 baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
            u8 sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
            emitInstruction(thisTAC, context, "\ts%c %s, %d(%s)\n",
                            SelectWidthChar(scope, &thisTAC->operands[0]),
                            registerNames[sourceReg],
                            thisTAC->operands[1].name.val,
                            registerNames[baseReg]);
        }
        break;

        case tt_store_arr:
        {
            u8 baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
            u8 offsetReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
            u8 sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[3], reservedRegisters[2]);

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
            u8 addrReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
            addrReg = placeAddrOfLifetimeInReg(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], addrReg);
            WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], addrReg);
        }
        break;

        case tt_lea_off:
        {
            // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
            u8 destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
            u8 baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);

            emitInstruction(thisTAC, context, "\taddi %s, %s, %d\n",
                            registerNames[destReg],
                            registerNames[baseReg],
                            thisTAC->operands[2].name.val);

            WriteVariable(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_lea_arr:
        {
            u8 destReg = pickWriteRegister(scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);
            u8 baseReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[1]);
            u8 offsetReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[2]);

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
            u8 operand1register = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[1], reservedRegisters[0]);
            u8 operand2register = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[2], reservedRegisters[1]);
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
            u8 sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], reservedRegisters[0]);

            EmitStackStoreForSize(thisTAC,
                                  context,
                                  sourceReg,
                                  Type_GetSize(TAC_GetTypeOfOperand(thisTAC, 0), scope),
                                  thisTAC->operands[1].name.val);
        }
        break;

        case tt_function_call:
        {
            struct FunctionEntry *calledFunction = lookupFunByString(scope, thisTAC->operands[1].name.str);
            if (calledFunction->isDefined)
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

        case tt_method_call:
        {
            struct ClassEntry *methodOf = lookupClassByType(scope, TAC_GetTypeOfOperand(thisTAC, 2));
            struct FunctionEntry *calledMethod = lookupMethodByString(methodOf, thisTAC->operands[1].name.str);
            // TODO: member function name mangling/uniqueness
            if (calledMethod->isDefined)
            {
                emitInstruction(thisTAC, context, "\tcall %s_%s\n", methodOf->name, thisTAC->operands[1].name.str);
            }
            else
            {
                emitInstruction(thisTAC, context, "\tcall %s_%s@plt\n", methodOf->name, thisTAC->operands[1].name.str);
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
                u8 sourceReg = placeOrFindOperandInRegister(thisTAC, context, scope, lifetimes, &thisTAC->operands[0], RETURN_REGISTER);

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
        case tt_phi:
            break;
        }
    }
}
