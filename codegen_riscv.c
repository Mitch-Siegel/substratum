#include "codegen_riscv.h"

#include "codegen_generic.h"
#include "log.h"
#include "symtab.h"
#include "tac.h"

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the frame pointer
void riscv_EmitFrameStoreForSize(struct TACLine *correspondingTACLine,
                                 struct CodegenState *state,
                                 struct MachineInfo *machineInfo,
                                 struct Register *sourceReg,
                                 u8 size,
                                 ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\ts%c %s, %d(%s)\n", SelectWidthCharForSize(size), sourceReg->name, offset, machineInfo->framePointer->name);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the frame pointer
void riscv_EmitFrameLoadForSize(struct TACLine *correspondingTACLine,
                                struct CodegenState *state,
                                struct MachineInfo *machineInfo,
                                struct Register *destReg,
                                u8 size,
                                ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\tl%c %s, %d(%s)\n", SelectWidthCharForSize(size), destReg->name, offset, machineInfo->framePointer->name);
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the stack pointer
void riscv_EmitStackStoreForSize(struct TACLine *correspondingTACLine,
                                 struct CodegenState *state,
                                 struct MachineInfo *machineInfo,
                                 struct Register *sourceReg,
                                 u8 size,
                                 ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\ts%c %s, %d(sp)\n", SelectWidthCharForSize(size), sourceReg->name, offset, machineInfo->stackPointer->name);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the stack pointer
void riscv_EmitStackLoadForSize(struct TACLine *correspondingTACLine,
                                struct CodegenState *state,
                                struct MachineInfo *machineInfo,
                                struct Register *destReg,
                                u8 size,
                                ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\tl%c %s, %d(sp)\n", SelectWidthCharForSize(size), destReg->name, offset, machineInfo->stackPointer->name);
}

void riscv_EmitPushForOperand(struct TACLine *correspondingTACLine,
                              struct CodegenState *state,
                              struct Scope *scope,
                              struct TACOperand *dataSource,
                              struct Register *srcRegister)
{
    InternalError("riscv_EmitPushForOperand not implemented");
    // size_t size = Type_GetSize(TACOperand_GetType(dataSource), scope);
    // switch (size)
    // {
    // case sizeof(u8):
    // case sizeof(u16):
    // case sizeof(u32):
    // case sizeof(u64):
    //     riscv_EmitPushForSize(correspondingTACLine, state, size, srcRegister);
    //     break;

    // default:
    // {
    //     char *typeName = Type_GetName(TACOperand_GetType(dataSource));
    //     InternalError("Unsupported size %zu seen in riscv_EmitPushForOperand (for type %s)", size, typeName);
    // }
    // }
}

void riscv_EmitPushForSize(struct TACLine *correspondingTACLine,
                           struct CodegenState *state,
                           u8 size,
                           struct Register *srcRegister)
{
    InternalError("riscv_EmitPushForSize not implemented");
    // TODO: reimplement with new register allocation
    // emitInstruction(correspondingTACLine, state, "\taddi sp, sp, -%d\n", size);
    // emitInstruction(correspondingTACLine, state, "\ts%c %s, 0(sp)\n",
    //                 SelectWidthCharForSize(size),
    //                 registerNames[srcRegister]);
}

void riscv_EmitPopForOperand(struct TACLine *correspondingTACLine,
                             struct CodegenState *state,
                             struct Scope *scope,
                             struct TACOperand *dataDest,
                             struct Register *destRegister)

{
    InternalError("riscv_EmitPopForOperand not implemented");
    // TODO: reimplement with new register allocation
    // size_t size = Type_GetSize(TACOperand_GetType(dataDest), scope);
    // switch (size)
    // {
    // case sizeof(u8):
    // case sizeof(u16):
    // case sizeof(u32):
    // case sizeof(u64):
    //     riscv_EmitPopForSize(correspondingTACLine, state, size, destRegister);

    //     break;

    // default:
    // {
    //     char *typeName = Type_GetName(TACOperand_GetType(dataDest));
    //     InternalError("Unsupported size %zu seen in riscv_EmitPopForOperand (for type %s)", size, typeName);
    // }
    // }
}

void riscv_EmitPopForSize(struct TACLine *correspondingTACLine,
                          struct CodegenState *state,
                          u8 size,
                          struct Register *destRegister)
{
    InternalError("riscv_EmitPopForSize not implemented");
    // TODO: reimplement with new register allocation
    // emitInstruction(correspondingTACLine, state, "\tl%c%s %s, 0(sp)\n",
    //                 SelectWidthCharForSize(size),
    //                 (size == MACHINE_REGISTER_SIZE_BYTES) ? "" : "u", // always generate an unsigned load (except for when loading 64 bit values, for which there is no unsigned load)
    //                 registerNames[destRegister]);
    // emitInstruction(correspondingTACLine, state, "\taddi sp, sp, %d\n", size);
}

void riscv_callerSaveRegisters(struct CodegenState *context, struct CodegenMetadata *metadata, struct MachineInfo *info)
{
    Log(LOG_DEBUG, "Caller-saving registers");
    struct Stack *actuallyCallerSaved = Stack_New();

    for (size_t regIndex = 0; regIndex < info->n_caller_save; regIndex++)
    {
        struct Register *potentiallyCallerSaved = info->caller_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (Set_Find(metadata->touchedRegisters, potentiallyCallerSaved) != NULL)
        {
            Stack_Push(actuallyCallerSaved, potentiallyCallerSaved);
        }
    }

    char *spName = info->stackPointer->name;
    emitInstruction(NULL, context, "subi %s, %s, %zd", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCallerSaved->size);
    for (size_t regIndex = 0; regIndex < actuallyCallerSaved->size; regIndex++)
    {
        struct Register *calleeSaved = info->callee_save[regIndex];
        riscv_EmitStackStoreForSize(NULL, context, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (regIndex + 1) * MACHINE_REGISTER_SIZE_BYTES));
    }

    Stack_Free(actuallyCallerSaved);
}

void riscv_callerRestoreRegisters(struct CodegenState *context, struct CodegenMetadata *metadata, struct MachineInfo *info)
{
    // TODO: implement for new register allocator
    Log(LOG_DEBUG, "Caller-restoring registers");
    struct Stack *actuallyCallerSaved = Stack_New();

    for (size_t regIndex = 0; regIndex < info->n_caller_save; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = info->caller_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (Set_Find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            Stack_Push(actuallyCallerSaved, potentiallyCalleeSaved);
        }
    }

    char *spName = info->stackPointer->name;
    for (size_t regIndex = 0; regIndex < actuallyCallerSaved->size; regIndex++)
    {
        struct Register *calleeSaved = info->caller_save[regIndex];
        riscv_EmitStackLoadForSize(NULL, context, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (regIndex + 1) * MACHINE_REGISTER_SIZE_BYTES));
    }
    emitInstruction(NULL, context, "addi %s, %s, %zd", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCallerSaved->size);

    Stack_Free(actuallyCallerSaved);
}

void riscv_calleeSaveRegisters(struct CodegenState *context, struct CodegenMetadata *metadata, struct MachineInfo *info)
{
    Log(LOG_DEBUG, "Callee-saving registers");
    struct Stack *actuallyCalleeSaved = Stack_New();

    for (size_t regIndex = 0; regIndex < info->n_caller_save; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = info->callee_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (Set_Find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            Stack_Push(actuallyCalleeSaved, potentiallyCalleeSaved);
        }
    }

    char *spName = info->stackPointer->name;
    emitInstruction(NULL, context, "subi %s, %s, %zd", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCalleeSaved->size);
    for (size_t regIndex = 0; regIndex < actuallyCalleeSaved->size; regIndex++)
    {
        struct Register *calleeSaved = info->callee_save[regIndex];
        riscv_EmitStackStoreForSize(NULL, context, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (regIndex + 1) * MACHINE_REGISTER_SIZE_BYTES));
    }

    Stack_Free(actuallyCalleeSaved);
}

void riscv_calleeRestoreRegisters(struct CodegenState *state, struct CodegenMetadata *metadata, struct MachineInfo *info)
{
    // TODO: implement for new register allocator
    Log(LOG_DEBUG, "Callee-restoring registers");
    struct Stack *actuallyCalleeSaved = Stack_New();

    for (size_t regIndex = 0; regIndex < info->n_caller_save; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = info->callee_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (Set_Find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            Stack_Push(actuallyCalleeSaved, potentiallyCalleeSaved);
        }
    }

    char *spName = info->stackPointer->name;
    for (size_t regIndex = 0; regIndex < actuallyCalleeSaved->size; regIndex++)
    {
        struct Register *calleeSaved = info->callee_save[regIndex];
        riscv_EmitStackLoadForSize(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (regIndex + 1) * MACHINE_REGISTER_SIZE_BYTES));
    }
    emitInstruction(NULL, state, "addi %s, %s, %zd", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCalleeSaved->size);

    Stack_Free(actuallyCalleeSaved);
}

void riscv_emitPrologue(struct CodegenState *context, struct CodegenMetadata *metadata, struct MachineInfo *info)
{
    // TODO: implement for new register allocator
}

void riscv_emitEpilogue(struct CodegenState *context, struct CodegenMetadata *metadata, struct MachineInfo *info, char *functionName)
{
    // TODO: implement for new register allocator
}

void riscv_WriteVariable(struct TACLine *correspondingTACLine,
                         struct CodegenState *state,
                         struct CodegenMetadata *metadata,
                         struct MachineInfo *info,
                         struct TACOperand *writtenTo,
                         struct Register *dataSource)
{
    // TODO: reimplement with new register allocation

    struct Lifetime dummyLifetime;
    memset(&dummyLifetime, 0, sizeof(struct Lifetime));
    dummyLifetime.name = writtenTo->name.str;

    struct Lifetime *writtenLifetime = Set_Find(metadata->allLifetimes, &dummyLifetime);
    if (writtenLifetime == NULL)
    {
        InternalError("No lifetime found for %s", dummyLifetime.name);
    }

    switch (writtenLifetime->wbLocation)
    {
    case wb_register:
        // only need to emit a move if the register locations differ
        if (writtenLifetime->writebackInfo.regLocation->index != dataSource->index)
        {
            emitInstruction(correspondingTACLine, state, "mv %s %s", writtenLifetime->writebackInfo.regLocation->name, dataSource->name);
            // TODO: generic way to track register occupancy at machine context level?
            writtenLifetime->writebackInfo.regLocation->containedLifetime = writtenLifetime;
        }
        break;

    case wb_stack:
    {
        riscv_EmitFrameStoreForSize(correspondingTACLine, state, info, dataSource, Type_GetSize(TACOperand_GetType(writtenTo), metadata->function->mainScope), writtenLifetime->writebackInfo.stackOffset);
    }
    break;

    case wb_global:
        break;

    case wb_unknown:
        InternalError("Lifetime for %s has unknown writeback location!", writtenLifetime->name);
    }
}

char *riscv_PlaceLiteralStringInRegister(struct TACLine *correspondingTACLine,
                                         struct CodegenState *context,
                                         char *literalStr,
                                         u8 destReg)
{
    // TODO: reimplement with new register allocation
    // char *destRegStr = registerNames[destReg];
    // emitInstruction(correspondingTACLine, context, "\tli %s, %s # place literal\n", destRegStr, literalStr);
    // return destRegStr;
    return NULL;
}

void riscv_placeAddrOfOperandInReg(struct TACLine *correspondingTACLine,
                                   struct CodegenState *state,
                                   struct CodegenMetadata *metadata,
                                   struct MachineInfo *info,
                                   struct TACOperand *operand,
                                   struct Register *destReg)
{
    struct Lifetime dummyLt = {0};
    dummyLt.name = operand->name.str;

    struct Lifetime *lifetime = Set_Find(metadata->allLifetimes, &dummyLt);
    switch (lifetime->wbLocation)
    {
    case wb_register:
        InternalError("placeAddrOfOperandInReg called on register lifetime %s", lifetime->name);
        break;

    case wb_stack:
        emitInstruction(correspondingTACLine, state, "addi %s, %s, %zd", destReg->name, info->framePointer->name, lifetime->writebackInfo.stackOffset);
        break;

    case wb_global:
        InternalError("placeAddrOfOperandInReg not supported for globals yet");
        break;

    case wb_unknown:
        InternalError("Lifetime for %s has unknown writeback location!", lifetime->name);
        break;
    }
}

void riscv_GenerateCodeForBasicBlock(struct CodegenState *state,
                                     struct CodegenMetadata *metadata,
                                     struct MachineInfo *info,
                                     struct BasicBlock *block,
                                     char *functionName)
{
    // we may pass null if we are generating the code to initialize global variables
    if (functionName != NULL)
    {
        fprintf(state->outFile, "%s_%zu:\n", functionName, block->labelNum);
    }

    size_t lastLineNo = 0;
    for (struct LinkedListNode *TACRunner = block->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
    {
        struct TACLine *thisTAC = TACRunner->data;

        char *printedTAC = sPrintTACLine(thisTAC);
        fprintf(state->outFile, "#%s\n", printedTAC);
        free(printedTAC);

        emitLoc(state, thisTAC, &lastLineNo);

        switch (thisTAC->operation)
        {
        case tt_asm:
            fputs(thisTAC->operands[0].name.str, state->outFile);
            fputc('\n', state->outFile);
            break;

        case tt_assign:
        {
            // only works for primitive types that will fit in registers
            struct Register *opLocReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));
            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], opLocReg);
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
            struct Register *op1Reg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));
            struct Register *op2Reg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[2], selectScratchRegister(info, false));
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], selectScratchRegister(info, true));

            // FIXME: work with new register allocation
            emitInstruction(thisTAC, state, "\t%s %s, %s, %s\n", getAsmOp(thisTAC->operation), destReg->name, op1Reg->name, op2Reg->name);
            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_bitwise_not:
        {
            struct Register *op1Reg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], selectScratchRegister(info, true));

            // FIXME: work with new register allocation
            emitInstruction(thisTAC, state, "\txori %s, %s, -1\n", destReg->name, op1Reg->name);
            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_load:
        {
            struct Register *baseReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], selectScratchRegister(info, true));

            char loadWidth = SelectWidthChar(metadata->scope, &thisTAC->operands[0]);
            emitInstruction(thisTAC, state, "\tl%c%s %s, 0(%s)\n",
                            loadWidth,
                            SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 0)),
                            destReg->name,
                            baseReg->name);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_load_off:
        {
            // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], selectScratchRegister(info, false));
            struct Register *baseReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, true));

            char loadWidth = SelectWidthChar(metadata->scope, &thisTAC->operands[0]);
            emitInstruction(thisTAC, state, "\tl%c%s %s, %d(%s)\n",
                            loadWidth,
                            SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 0)),
                            destReg->name,
                            thisTAC->operands[2].name.val,
                            baseReg->name);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_load_arr:
        {
            struct Register *baseReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));
            struct Register *offsetReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[2], selectScratchRegister(info, false));
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], selectScratchRegister(info, true));

            // TODO: check for shift by 0 and don't shift when applicable
            // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tslli %s, %s, %d\n",
                            offsetReg->name,
                            offsetReg->name,
                            thisTAC->operands[3].name.val);

            // add our scaled offset to the base address, put the full address into selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tadd %s, %s, %s\n",
                            baseReg->name,
                            baseReg->name,
                            offsetReg->name);

            char loadWidth = SelectWidthCharForDereference(metadata->scope, &thisTAC->operands[1]);
            emitInstruction(thisTAC, state, "\tl%c%s %s, 0(%s)\n",
                            loadWidth,
                            SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 1)),
                            destReg->name,
                            baseReg->name);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_store:
        {
            struct Register *destAddrReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[0], selectScratchRegister(info, false));
            struct Register *sourceReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));
            char storeWidth = SelectWidthCharForDereference(metadata->scope, &thisTAC->operands[0]);

            emitInstruction(thisTAC, state, "\ts%c %s, 0(%s)\n",
                            storeWidth,
                            sourceReg->name,
                            destAddrReg->name);
        }
        break;

        case tt_store_off:
        {
            // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
            struct Register *baseReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[0], selectScratchRegister(info, false));
            struct Register *sourceReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[2], selectScratchRegister(info, false));
            emitInstruction(thisTAC, state, "\ts%c %s, %d(%s)\n",
                            SelectWidthChar(metadata->scope, &thisTAC->operands[0]),
                            sourceReg->name,
                            thisTAC->operands[1].name.val,
                            baseReg->name);
        }
        break;

        case tt_store_arr:
        {
            struct Register *baseReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[0], selectScratchRegister(info, false));
            struct Register *offsetReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));

            // TODO: check for shift by 0 and don't shift when applicable
            // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tslli %s, %s, %d\n",
                            offsetReg->name,
                            offsetReg->name,
                            thisTAC->operands[2].name.val);

            // add our scaled offset to the base address, put the full address into selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tadd %s, %s, %s\n",
                            baseReg->name,
                            baseReg->name,
                            offsetReg->name);

            struct Register *sourceReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[3], selectScratchRegister(info, false));
            emitInstruction(thisTAC, state, "\ts%c %s, 0(%s)\n",
                            SelectWidthCharForDereference(metadata->scope, &thisTAC->operands[0]),
                            sourceReg->name,
                            baseReg->name);
        }
        break;

        case tt_addrof:
        {
            struct Register *addrReg = pickWriteRegister(metadata, &thisTAC->operands[0], selectScratchRegister(info, false));
            riscv_placeAddrOfOperandInReg(thisTAC, state, metadata, info, &thisTAC->operands[1], addrReg);
            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], addrReg);
        }
        break;

        case tt_lea_off:
        {
            // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], selectScratchRegister(info, false));
            struct Register *baseReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));

            emitInstruction(thisTAC, state, "\taddi %s, %s, %d\n",
                            destReg->name,
                            baseReg->name,
                            thisTAC->operands[2].name.val);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_lea_arr:
        {
            struct Register *baseReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));
            struct Register *offsetReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[2], selectScratchRegister(info, false));

            // TODO: check for shift by 0 and don't shift when applicable
            // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tslli %s, %s, %d\n",
                            offsetReg->name,
                            offsetReg->name,
                            thisTAC->operands[3].name.val);

            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], selectScratchRegister(info, true));
            // add our scaled offset to the base address, put the full address into destReg
            emitInstruction(thisTAC, state, "\tadd %s, %s, %s\n",
                            destReg->name,
                            baseReg->name,
                            offsetReg->name);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
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
            struct Register *operand1register = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[1], selectScratchRegister(info, false));
            struct Register *operand2register = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[2], selectScratchRegister(info, false));
            emitInstruction(thisTAC, state, "\t%s %s, %s, %s_%d\n",
                            getAsmOp(thisTAC->operation),
                            operand1register->name,
                            operand2register->name,
                            functionName,
                            thisTAC->operands[0].name.val);
        }
        break;

        case tt_jmp:
        {
            emitInstruction(thisTAC, state, "\tj %s_%d\n", functionName, thisTAC->operands[0].name.val);
        }
        break;

        case tt_stack_reserve:
        {
            // FIXME: work with new register allocation
            // emitInstruction(thisTAC, state, "\taddi %s, %s, -%d\n", registerNames[sp], registerNames[sp], thisTAC->operands[0].name.val);
        }
        break;

        case tt_stack_store:
        {
            struct Register *sourceReg = placeOrFindOperandInRegister(thisTAC, state, &thisTAC->operands[0], selectScratchRegister(info, false));

            riscv_EmitStackStoreForSize(thisTAC,
                                        state,
                                        info,
                                        sourceReg,
                                        Type_GetSize(TAC_GetTypeOfOperand(thisTAC, 0), metadata->scope),
                                        thisTAC->operands[1].name.val);
        }
        break;

        case tt_function_call:
        {
            struct FunctionEntry *calledFunction = lookupFunByString(metadata->function->mainScope, thisTAC->operands[1].name.str);
            if (calledFunction->isDefined)
            {
                emitInstruction(thisTAC, state, "\tcall %s\n", thisTAC->operands[1].name.str);
            }
            else
            {
                emitInstruction(thisTAC, state, "\tcall %s@plt\n", thisTAC->operands[1].name.str);
            }

            if (thisTAC->operands[0].name.str != NULL)
            {
                // FIXME:
                // riscv_WriteVariable(thisTAC, state, metadata->scope, &thisTAC->operands[0], RETURN_REGISTER);
            }
        }
        break;

        case tt_method_call:
        {
            struct StructEntry *methodOf = lookupStructByType(metadata->scope, TAC_GetTypeOfOperand(thisTAC, 2));
            struct FunctionEntry *calledMethod = lookupMethodByString(methodOf, thisTAC->operands[1].name.str);
            // TODO: member function name mangling/uniqueness
            if (calledMethod->isDefined)
            {
                emitInstruction(thisTAC, state, "\tcall %s_%s\n", methodOf->name, thisTAC->operands[1].name.str);
            }
            else
            {
                emitInstruction(thisTAC, state, "\tcall %s_%s@plt\n", methodOf->name, thisTAC->operands[1].name.str);
            }

            if (thisTAC->operands[0].name.str != NULL)
            {
                // FIXME: make work with new register allocation
                // riscv_WriteVariable(thisTAC, state, metadata->scope, &thisTAC->operands[0], RETURN_REGISTER);
            }
        }
        break;

        case tt_label:
            fprintf(state->outFile, "\t%s_%ld:\n", functionName, thisTAC->operands[0].name.val);
            break;

        case tt_return:
        {
            if (thisTAC->operands[0].name.str != NULL)
            {
                // FIXME: make work with new register allocation
                // struct Register *sourceReg = placeOrFindOperandInRegister(thisTAC, state, metadata->scope, &thisTAC->operands[0], RETURN_REGISTER);

                // if (sourceReg != RETURN_REGISTER)
                // {
                //     emitInstruction(thisTAC, state, "\tmv %s, %s\n",
                //                     registerNames[RETURN_REGISTER],
                //                     registerNames[sourceReg]);
                // }
            }
            emitInstruction(thisTAC, state, "\tj %s_done\n", functionName);
        }
        break;

        case tt_do:
        case tt_enddo:
        case tt_phi:
            break;
        }
    }
}
