#include "codegen_riscv.h"

#include "codegen_generic.h"
#include "log.h"
#include "symtab.h"
#include "tac.h"

char riscv_SelectWidthCharForSize(u8 size)
{
    char widthChar = '\0';
    switch (size)
    {
    case sizeof(u8):
        widthChar = 'b';
        break;

    case sizeof(u16):
        widthChar = 'h';
        break;

    case sizeof(u32):
        widthChar = 'w';
        break;

    case sizeof(u64):
        widthChar = 'd';
        break;

    default:
        InternalError("Error in riscv_SelectWidth: Unexpected destination variable size\n\tVariable is not pointer, and is not of size 1, 2, 4, or 8 bytes!");
    }

    return widthChar;
}

const char *riscv_SelectSignForLoad(u8 loadSize, struct Type *loaded)
{
    switch (loadSize)
    {
    case 'b':
    case 'h':
    case 'w':
        return "u";

    case 'd':
        return "";

    default:
        InternalError("Unexpected load size character seen in riscv_SignForLoad!");
    }
}

char riscv_SelectWidthChar(struct Scope *scope, struct TACOperand *dataDest)
{
    // pointers and arrays (decay implicitly at this stage to pointers) are always full-width
    if (Type_GetIndirectionLevel(TACOperand_GetType(dataDest)) > 0)
    {
        return 'd';
    }

    return riscv_SelectWidthCharForSize(Type_GetSize(TACOperand_GetType(dataDest), scope));
}

char riscv_SelectWidthCharForDereference(struct Scope *scope, struct TACOperand *dataDest)
{
    struct Type *operandType = TACOperand_GetType(dataDest);
    if ((operandType->pointerLevel == 0) &&
        (operandType->basicType != vt_array))
    {
        InternalError("riscv_SelectWidthCharForDereference called on non-indirect operand %s!", dataDest->name.str);
    }
    struct Type dereferenced = *operandType;

    // if not a pointer, we are dereferenceing an array so jump one layer down
    if (dereferenced.pointerLevel == 0)
    {
        dereferenced = *dereferenced.array.type;
    }
    else
    {
        // is a pointer, decrement pointer level
        dereferenced.pointerLevel--;
    }
    return riscv_SelectWidthCharForSize(Type_GetSize(&dereferenced, scope));
}

char riscv_SelectWidthCharForLifetime(struct Scope *scope, struct Lifetime *lifetime)
{
    char widthChar = '\0';
    if (lifetime->type.pointerLevel > 0)
    {
        widthChar = 'd';
    }
    else
    {
        widthChar = riscv_SelectWidthCharForSize(Type_GetSize(&lifetime->type, scope));
    }

    return widthChar;
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the frame pointer
void riscv_EmitFrameStoreForSize(struct TACLine *correspondingTACLine,
                                 struct CodegenState *state,
                                 struct MachineInfo *info,
                                 struct Register *sourceReg,
                                 u8 size,
                                 ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\ts%c %s, %d(%s)\n", riscv_SelectWidthCharForSize(size), sourceReg->name, offset, info->framePointer->name);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the frame pointer
void riscv_EmitFrameLoadForSize(struct TACLine *correspondingTACLine,
                                struct CodegenState *state,
                                struct MachineInfo *info,
                                struct Register *destReg,
                                u8 size,
                                ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\tl%c %s, %d(%s)\n", riscv_SelectWidthCharForSize(size), destReg->name, offset, info->framePointer->name);
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the stack pointer
void riscv_EmitStackStoreForSize(struct TACLine *correspondingTACLine,
                                 struct CodegenState *state,
                                 struct MachineInfo *info,
                                 struct Register *sourceReg,
                                 u8 size,
                                 ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\ts%c %s, %d(sp)\n", riscv_SelectWidthCharForSize(size), sourceReg->name, offset, info->stackPointer->name);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the stack pointer
void riscv_EmitStackLoadForSize(struct TACLine *correspondingTACLine,
                                struct CodegenState *state,
                                struct MachineInfo *info,
                                struct Register *destReg,
                                u8 size,
                                ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\tl%c %s, %d(sp)\n", riscv_SelectWidthCharForSize(size), destReg->name, offset, info->stackPointer->name);
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
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, state, "\taddi sp, sp, -%d\n", size);
    emitInstruction(correspondingTACLine, state, "\ts%c %s, 0(sp)\n",
                    riscv_SelectWidthCharForSize(size),
                    srcRegister->name);
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
    emitInstruction(correspondingTACLine, state, "\tl%c%s %s, 0(sp)\n",
                    riscv_SelectWidthCharForSize(size),
                    (size == MACHINE_REGISTER_SIZE_BYTES) ? "" : "u", // always generate an unsigned load (except for when loading 64 bit values, for which there is no unsigned load)
                    destRegister->name);
    emitInstruction(correspondingTACLine, state, "\taddi sp, sp, %d\n", size);
}

void riscv_callerSaveRegisters(struct CodegenState *state, struct FunctionEntry *calledFunction, struct MachineInfo *info)
{
    struct RegallocMetadata *metadata = &calledFunction->regalloc;

    Log(LOG_DEBUG, "Caller-saving registers");
    struct Stack *actuallyCallerSaved = Stack_New();

    for (size_t regIndex = 0; regIndex < info->n_caller_save; regIndex++)
    {
        struct Register *potentiallyCallerSaved = info->caller_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (Set_Find(metadata->touchedRegisters, potentiallyCallerSaved) != NULL)
        {
            Stack_Push(actuallyCallerSaved, potentiallyCallerSaved);
            Log(LOG_DEBUG, "%s is used in %s, need to caller-save", potentiallyCallerSaved->name, metadata->function->name);
        }
    }

    if (actuallyCallerSaved->size == 0)
    {
        Stack_Free(actuallyCallerSaved);
        return;
    }

    emitInstruction(NULL, state, "\t#Caller-save registers\n");

    char *spName = info->stackPointer->name;
    emitInstruction(NULL, state, "\taddi %s, %s, -%zd\n", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCallerSaved->size);
    for (size_t regIndex = 0; regIndex < actuallyCallerSaved->size; regIndex++)
    {
        struct Register *calleeSaved = actuallyCallerSaved->data[regIndex];
        riscv_EmitStackStoreForSize(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, regIndex * MACHINE_REGISTER_SIZE_BYTES);
    }

    Stack_Free(actuallyCallerSaved);
}

void riscv_callerRestoreRegisters(struct CodegenState *state, struct FunctionEntry *calledFunction, struct MachineInfo *info)
{
    struct RegallocMetadata *metadata = &calledFunction->regalloc;

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

    if (actuallyCallerSaved->size == 0)
    {
        Stack_Free(actuallyCallerSaved);
        return;
    }

    emitInstruction(NULL, state, "\t#Caller-restore registers\n");

    char *spName = info->stackPointer->name;
    for (size_t regIndex = 0; regIndex < actuallyCallerSaved->size; regIndex++)
    {
        struct Register *calleeSaved = actuallyCallerSaved->data[regIndex];
        riscv_EmitStackLoadForSize(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (regIndex* MACHINE_REGISTER_SIZE_BYTES));
    }
    emitInstruction(NULL, state, "\taddi %s, %s, %zd\n", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCallerSaved->size);

    Stack_Free(actuallyCallerSaved);
}

void riscv_calleeSaveRegisters(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    Log(LOG_DEBUG, "Callee-saving registers");
    struct Stack *actuallyCalleeSaved = Stack_New();

    for (size_t regIndex = 0; regIndex < info->n_callee_save; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = info->callee_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (Set_Find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            Stack_Push(actuallyCalleeSaved, potentiallyCalleeSaved);
        }
    }

    if (actuallyCalleeSaved->size == 0)
    {
        Stack_Free(actuallyCalleeSaved);
        return;
    }

    emitInstruction(NULL, state, "\t#Callee-save registers\n");
    for (size_t regIndex = 0; regIndex < actuallyCalleeSaved->size; regIndex++)
    {
        struct Register *calleeSaved = actuallyCalleeSaved->data[regIndex];
        // +2, 1 to account for stack growing downward and 1 to account for saved frame pointer
        riscv_EmitFrameStoreForSize(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (regIndex + 2) * MACHINE_REGISTER_SIZE_BYTES));
    }

    Stack_Free(actuallyCalleeSaved);
}

void riscv_calleeRestoreRegisters(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    // TODO: implement for new register allocator
    Log(LOG_DEBUG, "Callee-restoring registers");
    struct Stack *actuallyCalleeSaved = Stack_New();

    for (size_t regIndex = 0; regIndex < info->n_callee_save; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = info->callee_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (Set_Find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            Stack_Push(actuallyCalleeSaved, potentiallyCalleeSaved);
        }
    }

    if (actuallyCalleeSaved->size == 0)
    {
        Stack_Free(actuallyCalleeSaved);
        return;
    }

    emitInstruction(NULL, state, "\t#Callee-restore registers\n");
    for (size_t regIndex = 0; regIndex < actuallyCalleeSaved->size; regIndex++)
    {
        struct Register *calleeSaved = actuallyCalleeSaved->data[regIndex];
        // +2, 1 to account for stack growing downward and 1 to account for saved frame pointer
        riscv_EmitFrameLoadForSize(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (regIndex + 2) * MACHINE_REGISTER_SIZE_BYTES));
    }

    Stack_Free(actuallyCalleeSaved);
}

void riscv_emitPrologue(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    fprintf(state->outFile, "\t.cfi_startproc\n");
    emitInstruction(NULL, state, "\t.cfi_def_cfa_offset %zd\n", (ssize_t)-1 * MACHINE_REGISTER_SIZE_BYTES);
    riscv_EmitStackStoreForSize(NULL, state, info, info->framePointer, MACHINE_REGISTER_SIZE_BYTES, (-1 * MACHINE_REGISTER_SIZE_BYTES));
    emitInstruction(NULL, state, "\tmv %s, %s\n", info->framePointer->name, info->stackPointer->name);

    emitInstruction(NULL, state, "\t#reserve space for locals and callee-saved registers\n");
    emitInstruction(NULL, state, "\taddi %s, %s, -%zu\n", info->stackPointer->name, info->stackPointer->name, metadata->localStackSize);

    riscv_calleeSaveRegisters(state, metadata, info);

    // TODO: implement for new register allocator
}

void riscv_emitEpilogue(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info, char *functionName)
{
    emitInstruction(NULL, state, "%s_done:\n", functionName);
    riscv_calleeRestoreRegisters(state, metadata, info);

    emitInstruction(NULL, state, "\taddi %s, %s, %zu\n", info->stackPointer->name, info->stackPointer->name, metadata->localStackSize);

    riscv_EmitFrameLoadForSize(NULL, state, info, info->framePointer, MACHINE_REGISTER_SIZE_BYTES, (-1 * MACHINE_REGISTER_SIZE_BYTES));

    emitInstruction(NULL, state, "\taddi %s, %s, -%zd\n", info->stackPointer->name, info->stackPointer->name, metadata->argStackSize);

    emitInstruction(NULL, state, "\tjalr zero, 0(%s)\n", info->returnAddress->name);
    fprintf(state->outFile, "\t.cfi_endproc\n");

    // TODO: implement for new register allocator
}

// places an operand by name into the specified register, or returns the index of the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
struct Register *riscv_placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
                                                    struct CodegenState *state,
                                                    struct RegallocMetadata *metadata,
                                                    struct MachineInfo *info,
                                                    struct TACOperand *operand,
                                                    struct Register *optionalScratch)
{
    verifyCodegenPrimitive(operand);

    if (operand->permutation == vp_literal)
    {
        if (optionalScratch == NULL)
        {
            InternalError("Expected scratch register to place literal in, didn't get one!");
        }

        riscv_PlaceLiteralStringInRegister(correspondingTACLine, state, operand->name.str, optionalScratch);
        return optionalScratch;
    }

    struct Register *placedOrFoundIn = NULL;
    struct Lifetime dummyLt = {0};
    dummyLt.name = operand->name.str;

    struct Lifetime *operandLt = Set_Find(metadata->allLifetimes, &dummyLt);
    if (operandLt == NULL)
    {
        InternalError("Unable to find lifetime for variable %s!", operand->name.str);
    }

    switch (operandLt->wbLocation)
    {
    case wb_register:
        placedOrFoundIn = operandLt->writebackInfo.regLocation;
        break;

    case wb_stack:
        if (operandLt->type.basicType == vt_array)
        {
            if (operandLt->writebackInfo.stackOffset >= 0)
            {
                emitInstruction(correspondingTACLine, state, "\taddi %s, fp, %d # place %s\n", optionalScratch->name, operandLt->writebackInfo.stackOffset, operand->name.str);
            }
            else
            {
                emitInstruction(correspondingTACLine, state, "\taddi %s, fp, -%d # place %s\n", optionalScratch->name, -1 * operandLt->writebackInfo.stackOffset, operand->name.str);
            }
            placedOrFoundIn = optionalScratch;
        }
        else
        {
            riscv_EmitFrameLoadForSize(correspondingTACLine, state, info, optionalScratch, Type_GetSize(TACOperand_GetType(operand), metadata->scope), operandLt->writebackInfo.stackOffset);
            placedOrFoundIn = optionalScratch;
        }
        break;

    case wb_global:
    {
        if (optionalScratch == NULL)
        {
            InternalError("Expected scratch register to place global in, didn't get one!");
        }

        char loadWidth = 'X';
        const char *loadSign = "";

        if (operandLt->type.basicType == vt_array)
        {
            // if array, treat as pointer
            loadWidth = 'd';
        }
        else
        {
            loadWidth = riscv_SelectWidthCharForLifetime(metadata->scope, operandLt);
            loadSign = riscv_SelectSignForLoad(loadWidth, &operandLt->type);
        }

        placedOrFoundIn = optionalScratch;
        emitInstruction(correspondingTACLine, state, "\tla %s, %s # place %s\n",
                        placedOrFoundIn->name,
                        operandLt->name,
                        operand->name.str);

        if (operandLt->type.basicType != vt_array)
        {
            emitInstruction(correspondingTACLine, state, "\tl%c%s %s, 0(%s) # place %s\n",
                            loadWidth,
                            loadSign,
                            placedOrFoundIn->name,
                            placedOrFoundIn->name,
                            operand->name.str);
        }
    }
    break;

    case wb_unknown:
        InternalError("Unknown writeback location seen for lifetime %s", operandLt->name);
        break;
    }

    return placedOrFoundIn;
}

void riscv_WriteVariable(struct TACLine *correspondingTACLine,
                         struct CodegenState *state,
                         struct RegallocMetadata *metadata,
                         struct MachineInfo *info,
                         struct TACOperand *writtenTo,
                         struct Register *dataSource)
{
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
            emitInstruction(correspondingTACLine, state, "\t#write register variable %s\n", writtenLifetime->name);
            emitInstruction(correspondingTACLine, state, "\tmv %s, %s\n", writtenLifetime->writebackInfo.regLocation->name, dataSource->name);
            writtenLifetime->writebackInfo.regLocation->containedLifetime = writtenLifetime;
        }
        break;

    case wb_stack:
    {
        riscv_EmitFrameStoreForSize(correspondingTACLine, state, info, dataSource, Type_GetSize(TACOperand_GetType(writtenTo), metadata->function->mainScope), writtenLifetime->writebackInfo.stackOffset);
    }
    break;

    case wb_global:
    {
        u8 width = riscv_SelectWidthChar(metadata->scope, writtenTo);
        struct Register *addrReg = acquireScratchRegister(info);

        emitInstruction(correspondingTACLine, state, "\t# Write (global) variable %s\n", writtenLifetime->name);
        emitInstruction(correspondingTACLine, state, "\tla %s, %s\n",
                        addrReg->name,
                        writtenLifetime->name);

        emitInstruction(correspondingTACLine, state, "\ts%c %s, 0(%s)\n",
                        width,
                        dataSource->name,
                        addrReg->name);
    }
    break;

    case wb_unknown:
        InternalError("Lifetime for %s has unknown writeback location!", writtenLifetime->name);
    }
}

void riscv_PlaceLiteralStringInRegister(struct TACLine *correspondingTACLine,
                                        struct CodegenState *state,
                                        char *literalStr,
                                        struct Register *destReg)
{
    emitInstruction(correspondingTACLine, state, "\tli %s, %s # place literal\n", destReg->name, literalStr);
}

void riscv_placeAddrOfOperandInReg(struct TACLine *correspondingTACLine,
                                   struct CodegenState *state,
                                   struct RegallocMetadata *metadata,
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
        emitInstruction(correspondingTACLine, state, "addi %s, %s, %zd\n", destReg->name, info->framePointer->name, lifetime->writebackInfo.stackOffset);
        break;

    case wb_global:
        emitInstruction(correspondingTACLine, state, "\tla %s, %s\n", destReg->name, lifetime->name);
        break;

    case wb_unknown:
        InternalError("Lifetime for %s has unknown writeback location!", lifetime->name);
        break;
    }
}

void riscv_emitArgumentStores(struct CodegenState *state,
                              struct RegallocMetadata *metadata,
                              struct MachineInfo *info,
                              struct FunctionEntry *calledFunction,
                              struct Stack *argumentOperands)
{
    struct Register *postCallFramePointer = acquireScratchRegister(info);
    // TODO: constant/define for number of saved registers which aren't caught generally by the calling convention? Or sort out having the calling convention deal with RA/FP storing
    emitInstruction(NULL, state, "\taddi %s, %s, -%zd\n", postCallFramePointer->name, info->stackPointer->name, calledFunction->regalloc.argStackSize + (2 * MACHINE_REGISTER_SIZE_BYTES));

    while (argumentOperands->size > 0)
    {
        struct TACOperand *argOperand = Stack_Pop(argumentOperands);

        struct VariableEntry *argument = calledFunction->arguments->data[argumentOperands->size];

        struct Lifetime dummyLt = {0};
        dummyLt.name = argument->name;
        struct Lifetime *argLifetime = Set_Find(calledFunction->regalloc.allLifetimes, &dummyLt);

        emitInstruction(NULL, state, "\t#Store argument %s - %s\n", argument->name, argOperand->name.str);
        switch (argLifetime->wbLocation)
        {
        case wb_register:
        {
            struct Register *writtenTo = argLifetime->writebackInfo.regLocation;
            struct Register *foundIn = riscv_placeOrFindOperandInRegister(NULL, state, metadata, info, argOperand, writtenTo);
            if (writtenTo != foundIn)
            {
                emitInstruction(NULL, state, "\tmv %s, %s\n", writtenTo->name, foundIn->name);
            }
            else
            {
                emitInstruction(NULL, state, "\t# already in %s\n", foundIn->name);
            }
        }
        break;

        case wb_stack:
        {
            struct Register *scratch = acquireScratchRegister(info);
            struct Register *writeFrom = riscv_placeOrFindOperandInRegister(NULL, state, metadata, info, argOperand, scratch);

            emitInstruction(NULL, state, "\ts%c %s, %zd(%s)\n",
                            riscv_SelectWidthCharForLifetime(calledFunction->mainScope, argLifetime),
                            writeFrom->name,
                            argLifetime->writebackInfo.stackOffset,
                            postCallFramePointer->name);

            tryReleaseScratchRegister(info, scratch);
        }
        break;

        case wb_global:
            InternalError("Lifetime for argument %s has global writeback!", argLifetime->name);
            break;

        case wb_unknown:
            InternalError("Lifetime for argument %s has unknown writeback!", argLifetime->name);
            break;
        }
    }
}

void riscv_GenerateCodeForBasicBlock(struct CodegenState *state,
                                     struct RegallocMetadata *metadata,
                                     struct MachineInfo *info,
                                     struct BasicBlock *block,
                                     char *functionName)
{
    // we may pass null if we are generating the code to initialize global variables
    if (functionName != NULL)
    {
        fprintf(state->outFile, "%s_%zu:\n", functionName, block->labelNum);
    }

    struct Stack *functionArguments = Stack_New();
    size_t lastLineNo = 0;
    for (struct LinkedListNode *TACRunner = block->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
    {
        releaseAllScratchRegisters(info);

        struct TACLine *thisTAC = TACRunner->data;

        char *printedTAC = sPrintTACLine(thisTAC);
        fprintf(state->outFile, "#%s\n", printedTAC);
        free(printedTAC);

        emitLoc(state, thisTAC, &lastLineNo);

        switch (thisTAC->operation)
        {
        case tt_asm:
            emitInstruction(thisTAC, state, "%s\n", thisTAC->operands[0].name.str);
            break;

        case tt_assign:
        {
            // only works for primitive types that will fit in registers
            struct Register *opLocReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));
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
            struct Register *op1Reg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));
            struct Register *op2Reg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[2], acquireScratchRegister(info));

            // release any scratch registers we may have acquired by placing operands, as our write register may be able to use one of them
            tryReleaseScratchRegister(info, op1Reg);
            tryReleaseScratchRegister(info, op2Reg);

            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], acquireScratchRegister(info));

            emitInstruction(thisTAC, state, "\t%s %s, %s, %s\n", getAsmOp(thisTAC->operation), destReg->name, op1Reg->name, op2Reg->name);
            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_bitwise_not:
        {
            struct Register *op1Reg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], acquireScratchRegister(info));

            // FIXME: work with new register allocation
            emitInstruction(thisTAC, state, "\txori %s, %s, -1\n", destReg->name, op1Reg->name);
            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_load:
        {
            struct Register *baseReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], acquireScratchRegister(info));

            char loadWidth = riscv_SelectWidthChar(metadata->scope, &thisTAC->operands[0]);
            emitInstruction(thisTAC, state, "\tl%c%s %s, 0(%s)\n",
                            loadWidth,
                            riscv_SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 0)),
                            destReg->name,
                            baseReg->name);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_load_off:
        {
            // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], acquireScratchRegister(info));
            struct Register *baseReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));

            char loadWidth = riscv_SelectWidthChar(metadata->scope, &thisTAC->operands[0]);
            emitInstruction(thisTAC, state, "\tl%c%s %s, %d(%s)\n",
                            loadWidth,
                            riscv_SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 0)),
                            destReg->name,
                            thisTAC->operands[2].name.val,
                            baseReg->name);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_load_arr:
        {
            struct Register *baseReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));
            struct Register *offsetReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[2], acquireScratchRegister(info));

            // because offsetReg may or may not be modifiable, we will immediately release it if it's a temp, and guarantee that shiftedOffsetReg is a temp that we can modify it to
            tryReleaseScratchRegister(info, offsetReg);
            struct Register *shiftedOffsetReg = acquireScratchRegister(info);

            // TODO: check for shift by 0 and don't shift when applicable
            // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tslli %s, %s, %d\n",
                            shiftedOffsetReg->name,
                            offsetReg->name,
                            thisTAC->operands[3].name.val);

            tryReleaseScratchRegister(info, baseReg);
            tryReleaseScratchRegister(info, shiftedOffsetReg);
            struct Register *addrReg = acquireScratchRegister(info);
            // add our scaled offset to the base address, put the full address into selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tadd %s, %s, %s\n",
                            addrReg->name,
                            baseReg->name,
                            shiftedOffsetReg->name);

            tryReleaseScratchRegister(info, shiftedOffsetReg);
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], acquireScratchRegister(info));
            char loadWidth = riscv_SelectWidthCharForDereference(metadata->scope, &thisTAC->operands[1]);
            emitInstruction(thisTAC, state, "\tl%c%s %s, 0(%s)\n",
                            loadWidth,
                            riscv_SelectSignForLoad(loadWidth, TAC_GetTypeOfOperand(thisTAC, 1)),
                            destReg->name,
                            addrReg->name);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_store:
        {
            struct Register *destAddrReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[0], acquireScratchRegister(info));
            struct Register *sourceReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));
            char storeWidth = riscv_SelectWidthCharForDereference(metadata->scope, &thisTAC->operands[0]);

            emitInstruction(thisTAC, state, "\ts%c %s, 0(%s)\n",
                            storeWidth,
                            sourceReg->name,
                            destAddrReg->name);
        }
        break;

        case tt_store_off:
        {
            // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
            struct Register *baseReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[0], acquireScratchRegister(info));
            struct Register *sourceReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[2], acquireScratchRegister(info));
            emitInstruction(thisTAC, state, "\ts%c %s, %d(%s)\n",
                            riscv_SelectWidthChar(metadata->scope, &thisTAC->operands[0]),
                            sourceReg->name,
                            thisTAC->operands[1].name.val,
                            baseReg->name);
        }
        break;

        case tt_store_arr:
        {
            struct Register *baseReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[0], acquireScratchRegister(info));
            struct Register *offsetReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));

            // TODO: check for shift by 0 and don't shift when applicable
            // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tslli %s, %s, %d\n",
                            offsetReg->name,
                            offsetReg->name,
                            thisTAC->operands[2].name.val);

            tryReleaseScratchRegister(info, baseReg);
            struct Register *addrReg = acquireScratchRegister(info);
            // add our scaled offset to the base address, put the full address into selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tadd %s, %s, %s\n",
                            addrReg->name,
                            baseReg->name,
                            offsetReg->name);

            tryReleaseScratchRegister(info, offsetReg);
            struct Register *sourceReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[3], acquireScratchRegister(info));
            emitInstruction(thisTAC, state, "\ts%c %s, 0(%s)\n",
                            riscv_SelectWidthCharForDereference(metadata->scope, &thisTAC->operands[0]),
                            sourceReg->name,
                            addrReg->name);
        }
        break;

        case tt_addrof:
        {
            struct Register *addrReg = pickWriteRegister(metadata, &thisTAC->operands[0], acquireScratchRegister(info));
            riscv_placeAddrOfOperandInReg(thisTAC, state, metadata, info, &thisTAC->operands[1], addrReg);
            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], addrReg);
        }
        break;

        case tt_lea_off:
        {
            // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], acquireScratchRegister(info));
            struct Register *baseReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));

            emitInstruction(thisTAC, state, "\taddi %s, %s, %d\n",
                            destReg->name,
                            baseReg->name,
                            thisTAC->operands[2].name.val);

            riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], destReg);
        }
        break;

        case tt_lea_arr:
        {
            struct Register *baseReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));
            struct Register *offsetReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[2], acquireScratchRegister(info));

            // because offsetReg may or may not be modifiable, we will immediately release it if it's a temp, and guarantee that shiftedOffsetReg is a temp that we can modify it to
            tryReleaseScratchRegister(info, offsetReg);
            struct Register *shiftedOffsetReg = acquireScratchRegister(info);

            // TODO: check for shift by 0 and don't shift when applicable
            // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
            emitInstruction(thisTAC, state, "\tslli %s, %s, %d\n",
                            shiftedOffsetReg->name,
                            offsetReg->name,
                            thisTAC->operands[3].name.val);

            // release any scratch registers we may have acquired by placing operands, as our write register may be able to use one of them
            releaseAllScratchRegisters(info);
            struct Register *destReg = pickWriteRegister(metadata, &thisTAC->operands[0], acquireScratchRegister(info));
            // add our scaled offset to the base address, put the full address into destReg
            emitInstruction(thisTAC, state, "\tadd %s, %s, %s\n",
                            destReg->name,
                            baseReg->name,
                            shiftedOffsetReg->name);

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
            struct Register *operand1register = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[1], acquireScratchRegister(info));
            struct Register *operand2register = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[2], acquireScratchRegister(info));
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

        case tt_arg_store:
        {
            Stack_Push(functionArguments, &thisTAC->operands[0]);
        }
        break;

        case tt_function_call:
        {
            struct FunctionEntry *calledFunction = lookupFunByString(metadata->function->mainScope, thisTAC->operands[1].name.str);

            riscv_callerSaveRegisters(state, calledFunction, info);

            riscv_emitArgumentStores(state, metadata, info, calledFunction, functionArguments);
            functionArguments->size = 0;

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
                riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], info->returnValue);
            }

            riscv_callerRestoreRegisters(state, calledFunction, info);
        }
        break;

        case tt_method_call:
        {
            struct StructEntry *methodOf = lookupStructByType(metadata->scope, TAC_GetTypeOfOperand(thisTAC, 2));
            struct FunctionEntry *calledMethod = lookupMethodByString(methodOf, thisTAC->operands[1].name.str);

            riscv_callerSaveRegisters(state, calledMethod, info);

            riscv_emitArgumentStores(state, metadata, info, calledMethod, functionArguments);
            functionArguments->size = 0;

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
                riscv_WriteVariable(thisTAC, state, metadata, info, &thisTAC->operands[0], info->returnValue);
            }

            riscv_callerRestoreRegisters(state, calledMethod, info);
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
                struct Register *sourceReg = riscv_placeOrFindOperandInRegister(thisTAC, state, metadata, info, &thisTAC->operands[0], info->returnValue);

                if (sourceReg != info->returnValue)
                {
                    emitInstruction(thisTAC, state, "\tmv %s, %s\n",
                                    info->returnValue->name,
                                    sourceReg->name);
                }
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

    Stack_Free(functionArguments);
}
