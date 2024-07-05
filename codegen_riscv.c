#include "codegen_riscv.h"

#include "codegen_generic.h"
#include "log.h"
#include "symtab.h"
#include "tac.h"

char riscv_select_width_char_for_size(u8 size)
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

const char *riscv_select_sign_for_load_char(char loadChar)
{
    switch (loadChar)
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

char riscv_select_width_char(struct Scope *scope, struct TACOperand *dataDest)
{
    // pointers and arrays (decay implicitly at this stage to pointers) are always full-width
    if (type_get_indirection_level(tac_operand_get_type(dataDest)) > 0)
    {
        return 'd';
    }

    return riscv_select_width_char_for_size(type_get_size(tac_operand_get_type(dataDest), scope));
}

char riscv_select_width_char_for_dereference(struct Scope *scope, struct TACOperand *dataDest)
{
    struct Type *operandType = tac_operand_get_type(dataDest);
    if ((operandType->pointerLevel == 0) &&
        (operandType->basicType != VT_ARRAY))
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
    return riscv_select_width_char_for_size(type_get_size(&dereferenced, scope));
}

char riscv_select_width_char_for_lifetime(struct Scope *scope, struct Lifetime *lifetime)
{
    char widthChar = '\0';
    if (lifetime->type.pointerLevel > 0)
    {
        widthChar = 'd';
    }
    else
    {
        widthChar = riscv_select_width_char_for_size(type_get_size(&lifetime->type, scope));
    }

    return widthChar;
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the frame pointer
void riscv_emit_frame_store_for_size(struct TACLine *correspondingTACLine,
                                     struct CodegenState *state,
                                     struct MachineInfo *info,
                                     struct Register *sourceReg,
                                     u8 size,
                                     ssize_t offset)
{
    emit_instruction(correspondingTACLine, state, "\ts%c %s, %d(%s)\n", riscv_select_width_char_for_size(size), sourceReg->name, offset, info->framePointer->name);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the frame pointer
void riscv_emit_frame_load_for_size(struct TACLine *correspondingTACLine,
                                    struct CodegenState *state,
                                    struct MachineInfo *info,
                                    struct Register *destReg,
                                    u8 size,
                                    ssize_t offset)
{
    emit_instruction(correspondingTACLine, state, "\tl%c %s, %d(%s)\n", riscv_select_width_char_for_size(size), destReg->name, offset, info->framePointer->name);
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the stack pointer
void riscv_emit_stack_store_for_size(struct TACLine *correspondingTACLine,
                                     struct CodegenState *state,
                                     struct MachineInfo *info,
                                     struct Register *sourceReg,
                                     u8 size,
                                     ssize_t offset)
{
    emit_instruction(correspondingTACLine, state, "\ts%c %s, %d(sp)\n", riscv_select_width_char_for_size(size), sourceReg->name, offset, info->stackPointer->name);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the stack pointer
void riscv_emit_stack_load_for_size(struct TACLine *correspondingTACLine,
                                    struct CodegenState *state,
                                    struct MachineInfo *info,
                                    struct Register *destReg,
                                    u8 size,
                                    ssize_t offset)
{
    emit_instruction(correspondingTACLine, state, "\tl%c %s, %d(sp)\n", riscv_select_width_char_for_size(size), destReg->name, offset, info->stackPointer->name);
}

void riscv_emit_push_for_size(struct TACLine *correspondingTACLine,
                              struct CodegenState *state,
                              u8 size,
                              struct Register *srcRegister)
{
    emit_instruction(correspondingTACLine, state, "\taddi sp, sp, -%d\n", size);
    emit_instruction(correspondingTACLine, state, "\ts%c %s, 0(sp)\n",
                     riscv_select_width_char_for_size(size),
                     srcRegister->name);
}

void riscv_emit_pop_for_size(struct TACLine *correspondingTACLine,
                             struct CodegenState *state,
                             u8 size,
                             struct Register *destRegister)
{
    emit_instruction(correspondingTACLine, state, "\tl%c%s %s, 0(sp)\n",
                     riscv_select_width_char_for_size(size),
                     (size == MACHINE_REGISTER_SIZE_BYTES) ? "" : "u", // always generate an unsigned load (except for when loading 64 bit values, for which there is no unsigned load)
                     destRegister->name);
    emit_instruction(correspondingTACLine, state, "\taddi sp, sp, %d\n", size);
}

void riscv_caller_save_registers(struct CodegenState *state, struct FunctionEntry *calledFunction, struct MachineInfo *info)
{
    struct RegallocMetadata *metadata = &calledFunction->regalloc;

    log(LOG_DEBUG, "Caller-saving registers");
    struct Stack *actuallyCallerSaved = stack_new();

    for (size_t regIndex = 0; regIndex < info->n_caller_save; regIndex++)
    {
        struct Register *potentiallyCallerSaved = info->caller_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (set_find(metadata->touchedRegisters, potentiallyCallerSaved) != NULL)
        {
            stack_push(actuallyCallerSaved, potentiallyCallerSaved);
            log(LOG_DEBUG, "%s is used in %s, need to caller-save", potentiallyCallerSaved->name, metadata->function->name);
        }
    }

    if (actuallyCallerSaved->size == 0)
    {
        stack_free(actuallyCallerSaved);
        return;
    }

    emit_instruction(NULL, state, "\t#Caller-save registers\n");

    char *spName = info->stackPointer->name;

    // TODO: don't emit when 0
    emit_instruction(NULL, state, "\taddi %s, %s, -%zd\n", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCallerSaved->size);

    for (ssize_t regIndex = 0; regIndex < actuallyCallerSaved->size; regIndex++)
    {
        struct Register *calleeSaved = actuallyCallerSaved->data[regIndex];
        riscv_emit_stack_store_for_size(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, regIndex * MACHINE_REGISTER_SIZE_BYTES);
    }

    stack_free(actuallyCallerSaved);
}

void riscv_caller_restore_registers(struct CodegenState *state, struct FunctionEntry *calledFunction, struct MachineInfo *info)
{
    struct RegallocMetadata *metadata = &calledFunction->regalloc;

    log(LOG_DEBUG, "Caller-restoring registers");
    struct Stack *actuallyCallerSaved = stack_new();

    for (size_t regIndex = 0; regIndex < info->n_caller_save; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = info->caller_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (set_find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            stack_push(actuallyCallerSaved, potentiallyCalleeSaved);
        }
    }

    if (actuallyCallerSaved->size == 0)
    {
        stack_free(actuallyCallerSaved);
        return;
    }

    emit_instruction(NULL, state, "\t#Caller-restore registers\n");

    char *spName = info->stackPointer->name;
    for (ssize_t regIndex = 0; regIndex < actuallyCallerSaved->size; regIndex++)
    {
        struct Register *calleeSaved = actuallyCallerSaved->data[regIndex];
        riscv_emit_stack_load_for_size(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (regIndex * MACHINE_REGISTER_SIZE_BYTES));
    }

    // TODO: don't emit when 0
    emit_instruction(NULL, state, "\taddi %s, %s, %zd\n", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCallerSaved->size);

    stack_free(actuallyCallerSaved);
}

void riscv_callee_save_registers(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    log(LOG_DEBUG, "Callee-saving registers");
    struct Stack *actuallyCalleeSaved = stack_new();

    for (size_t regIndex = 0; regIndex < info->n_callee_save; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = info->callee_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (set_find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            stack_push(actuallyCalleeSaved, potentiallyCalleeSaved);
        }
    }

    if (actuallyCalleeSaved->size == 0)
    {
        stack_free(actuallyCalleeSaved);
        return;
    }

    emit_instruction(NULL, state, "\t#Callee-save registers\n");
    for (ssize_t regIndex = 0; regIndex < actuallyCalleeSaved->size; regIndex++)
    {
        struct Register *calleeSaved = actuallyCalleeSaved->data[regIndex];
        // +2, 1 to account for stack growing downward and 1 to account for saved frame pointer
        riscv_emit_frame_store_for_size(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (regIndex + 2) * MACHINE_REGISTER_SIZE_BYTES));
    }

    stack_free(actuallyCalleeSaved);
}

void riscv_callee_restore_registers(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    log(LOG_DEBUG, "Callee-restoring registers");
    struct Stack *actuallyCalleeSaved = stack_new();

    for (size_t regIndex = 0; regIndex < info->n_callee_save; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = info->callee_save[regIndex];
        // only need to actually callee-save registers we touch in this function
        if (set_find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            stack_push(actuallyCalleeSaved, potentiallyCalleeSaved);
        }
    }

    if (actuallyCalleeSaved->size == 0)
    {
        stack_free(actuallyCalleeSaved);
        return;
    }

    emit_instruction(NULL, state, "\t#Callee-restore registers\n");
    for (ssize_t regIndex = 0; regIndex < actuallyCalleeSaved->size; regIndex++)
    {
        struct Register *calleeSaved = actuallyCalleeSaved->data[regIndex];
        // +2, 1 to account for stack growing downward and 1 to account for saved frame pointer
        riscv_emit_frame_load_for_size(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (regIndex + 2) * MACHINE_REGISTER_SIZE_BYTES));
    }

    stack_free(actuallyCalleeSaved);
}

void riscv_emit_prologue(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    fprintf(state->outFile, "\t.cfi_startproc\n");
    emit_instruction(NULL, state, "\t.cfi_def_cfa_offset %zd\n", (ssize_t)-1 * MACHINE_REGISTER_SIZE_BYTES);
    riscv_emit_stack_store_for_size(NULL, state, info, info->framePointer, MACHINE_REGISTER_SIZE_BYTES, ((ssize_t)-1 * MACHINE_REGISTER_SIZE_BYTES));
    emit_instruction(NULL, state, "\tmv %s, %s\n", info->framePointer->name, info->stackPointer->name);

    emit_instruction(NULL, state, "\t#reserve space for locals and callee-saved registers\n");
    emit_instruction(NULL, state, "\taddi %s, %s, -%zu\n", info->stackPointer->name, info->stackPointer->name, metadata->localStackSize);

    riscv_callee_save_registers(state, metadata, info);
}

void riscv_emit_epilogue(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info, char *functionName)
{
    emit_instruction(NULL, state, "%s_done:\n", functionName);
    riscv_callee_restore_registers(state, metadata, info);

    emit_instruction(NULL, state, "\taddi %s, %s, %zu\n", info->stackPointer->name, info->stackPointer->name, metadata->localStackSize);

    riscv_emit_frame_load_for_size(NULL, state, info, info->framePointer, MACHINE_REGISTER_SIZE_BYTES, ((ssize_t)-1 * MACHINE_REGISTER_SIZE_BYTES));

    // TODO: don't emit when 0
    emit_instruction(NULL, state, "\taddi %s, %s, -%zd\n", info->stackPointer->name, info->stackPointer->name, metadata->argStackSize);

    emit_instruction(NULL, state, "\tjalr zero, 0(%s)\n", info->returnAddress->name);
    fprintf(state->outFile, "\t.cfi_endproc\n");
}

// places an operand by name into the specified register, or returns the index of the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
struct Register *riscv_place_or_find_operand_in_register(struct TACLine *correspondingTACLine,
                                                         struct CodegenState *state,
                                                         struct RegallocMetadata *metadata,
                                                         struct MachineInfo *info,
                                                         struct TACOperand *operand,
                                                         struct Register *optionalScratch)
{
    verify_codegen_primitive(operand);

    if (operand->permutation == VP_LITERAL)
    {
        if (optionalScratch == NULL)
        {
            InternalError("Expected scratch register to place literal in, didn't get one!");
        }

        riscv_place_literal_string_in_register(correspondingTACLine, state, operand->name.str, optionalScratch);
        return optionalScratch;
    }

    struct Register *placedOrFoundIn = NULL;
    struct Lifetime dummyLt = {0};
    dummyLt.name = operand->name.str;

    struct Lifetime *operandLt = set_find(metadata->allLifetimes, &dummyLt);
    if (operandLt == NULL)
    {
        InternalError("Unable to find lifetime for variable %s!", operand->name.str);
    }

    switch (operandLt->wbLocation)
    {
    case WB_REGISTER:
        placedOrFoundIn = operandLt->writebackInfo.regLocation;
        break;

    case WB_STACK:
        if (operandLt->type.basicType == VT_ARRAY)
        {
            if (operandLt->writebackInfo.stackOffset >= 0)
            {
                emit_instruction(correspondingTACLine, state, "\taddi %s, fp, %d # place %s\n", optionalScratch->name, operandLt->writebackInfo.stackOffset, operand->name.str);
            }
            else
            {
                emit_instruction(correspondingTACLine, state, "\taddi %s, fp, -%d # place %s\n", optionalScratch->name, -1 * operandLt->writebackInfo.stackOffset, operand->name.str);
            }
            placedOrFoundIn = optionalScratch;
        }
        else
        {
            riscv_emit_frame_load_for_size(correspondingTACLine, state, info, optionalScratch, type_get_size(tac_operand_get_type(operand), metadata->scope), operandLt->writebackInfo.stackOffset);
            placedOrFoundIn = optionalScratch;
        }
        break;

    case WB_GLOBAL:
    {
        if (optionalScratch == NULL)
        {
            InternalError("Expected scratch register to place global in, didn't get one!");
        }

        char loadWidth = 'X';
        const char *loadSign = "";

        if (operandLt->type.basicType == VT_ARRAY)
        {
            // if array, treat as pointer
            loadWidth = 'd';
        }
        else
        {
            loadWidth = riscv_select_width_char_for_lifetime(metadata->scope, operandLt);
            loadSign = riscv_select_sign_for_load_char(loadWidth);
        }

        placedOrFoundIn = optionalScratch;
        emit_instruction(correspondingTACLine, state, "\tla %s, %s # place %s\n",
                         placedOrFoundIn->name,
                         operandLt->name,
                         operand->name.str);

        if (operandLt->type.basicType != VT_ARRAY)
        {
            emit_instruction(correspondingTACLine, state, "\tl%c%s %s, 0(%s) # place %s\n",
                             loadWidth,
                             loadSign,
                             placedOrFoundIn->name,
                             placedOrFoundIn->name,
                             operand->name.str);
        }
    }
    break;

    case WB_UNKNOWN:
        InternalError("Unknown writeback location seen for lifetime %s", operandLt->name);
        break;
    }

    return placedOrFoundIn;
}

void riscv_write_variable(struct TACLine *correspondingTACLine,
                          struct CodegenState *state,
                          struct RegallocMetadata *metadata,
                          struct MachineInfo *info,
                          struct TACOperand *writtenTo,
                          struct Register *dataSource)
{
    verify_codegen_primitive(writtenTo);

    struct Lifetime dummyLifetime;
    memset(&dummyLifetime, 0, sizeof(struct Lifetime));
    dummyLifetime.name = writtenTo->name.str;

    struct Lifetime *writtenLifetime = set_find(metadata->allLifetimes, &dummyLifetime);
    if (writtenLifetime == NULL)
    {
        InternalError("No lifetime found for %s", dummyLifetime.name);
    }

    switch (writtenLifetime->wbLocation)
    {
    case WB_REGISTER:
        // only need to emit a move if the register locations differ
        if (writtenLifetime->writebackInfo.regLocation->index != dataSource->index)
        {
            emit_instruction(correspondingTACLine, state, "\t#write register variable %s\n", writtenLifetime->name);
            emit_instruction(correspondingTACLine, state, "\tmv %s, %s\n", writtenLifetime->writebackInfo.regLocation->name, dataSource->name);
            writtenLifetime->writebackInfo.regLocation->containedLifetime = writtenLifetime;
        }
        break;

    case WB_STACK:
    {
        riscv_emit_frame_store_for_size(correspondingTACLine, state, info, dataSource, type_get_size(tac_operand_get_type(writtenTo), metadata->function->mainScope), writtenLifetime->writebackInfo.stackOffset);
    }
    break;

    case WB_GLOBAL:
    {
        u8 width = riscv_select_width_char(metadata->scope, writtenTo);
        struct Register *addrReg = acquire_scratch_register(info);

        emit_instruction(correspondingTACLine, state, "\t# Write (global) variable %s\n", writtenLifetime->name);
        emit_instruction(correspondingTACLine, state, "\tla %s, %s\n",
                         addrReg->name,
                         writtenLifetime->name);

        emit_instruction(correspondingTACLine, state, "\ts%c %s, 0(%s)\n",
                         width,
                         dataSource->name,
                         addrReg->name);
    }
    break;

    case WB_UNKNOWN:
        InternalError("Lifetime for %s has unknown writeback location!", writtenLifetime->name);
    }
}

void riscv_place_literal_string_in_register(struct TACLine *correspondingTACLine,
                                            struct CodegenState *state,
                                            char *literalStr,
                                            struct Register *destReg)
{
    emit_instruction(correspondingTACLine, state, "\tli %s, %s # place literal\n", destReg->name, literalStr);
}

void riscv_place_addr_of_operand_in_reg(struct TACLine *correspondingTACLine,
                                        struct CodegenState *state,
                                        struct RegallocMetadata *metadata,
                                        struct MachineInfo *info,
                                        struct TACOperand *operand,
                                        struct Register *destReg)
{
    struct Lifetime dummyLt = {0};
    dummyLt.name = operand->name.str;

    struct Lifetime *lifetime = set_find(metadata->allLifetimes, &dummyLt);
    switch (lifetime->wbLocation)
    {
    case WB_REGISTER:
        InternalError("placeAddrOfOperandInReg called on register lifetime %s", lifetime->name);
        break;

    case WB_STACK:
        emit_instruction(correspondingTACLine, state, "\taddi %s, %s, %zd # place address of %s in register\n", destReg->name, info->framePointer->name, lifetime->writebackInfo.stackOffset, lifetime->name);
        break;

    case WB_GLOBAL:
        emit_instruction(correspondingTACLine, state, "\tla %s, %s # place address of %s in register\n", destReg->name, lifetime->name, lifetime->name);
        break;

    case WB_UNKNOWN:
        InternalError("Lifetime for %s has unknown writeback location!", lifetime->name);
        break;
    }
}

void riscv_emit_argument_stores(struct CodegenState *state,
                                struct RegallocMetadata *metadata,
                                struct MachineInfo *info,
                                struct FunctionEntry *calledFunction,
                                struct Stack *argumentOperands)
{
    log(LOG_DEBUG, "Emit argument stores for call to %s", calledFunction->name);
    // TODO: don't emit when 0
    emit_instruction(NULL, state, "\taddi %s, %s, -%zd\n", info->stackPointer->name, info->stackPointer->name, calledFunction->regalloc.argStackSize);

    while (argumentOperands->size > 0)
    {
        struct TACOperand *argOperand = stack_pop(argumentOperands);

        struct VariableEntry *argument = calledFunction->arguments->data[argumentOperands->size];

        struct Lifetime dummyLt = {0};
        dummyLt.name = argument->name;
        struct Lifetime *argLifetime = set_find(calledFunction->regalloc.allLifetimes, &dummyLt);

        log(LOG_DEBUG, "Store argument %s - %s", argument->name, argOperand->name.str);
        emit_instruction(NULL, state, "\t#Store argument %s - %s\n", argument->name, argOperand->name.str);
        switch (argLifetime->wbLocation)
        {
        case WB_REGISTER:
        {
            struct Register *writtenTo = argLifetime->writebackInfo.regLocation;
            struct Register *foundIn = riscv_place_or_find_operand_in_register(NULL, state, metadata, info, argOperand, writtenTo);
            if (writtenTo != foundIn)
            {
                emit_instruction(NULL, state, "\tmv %s, %s\n", writtenTo->name, foundIn->name);
            }
            else
            {
                emit_instruction(NULL, state, "\t# already in %s\n", foundIn->name);
            }
        }
        break;

        case WB_STACK:
        {
            struct Register *scratch = acquire_scratch_register(info);
            struct Register *writeFrom = riscv_place_or_find_operand_in_register(NULL, state, metadata, info, argOperand, scratch);

            emit_instruction(NULL, state, "\ts%c %s, %zd(%s)\n",
                             riscv_select_width_char_for_lifetime(calledFunction->mainScope, argLifetime),
                             writeFrom->name,
                             argLifetime->writebackInfo.stackOffset,
                             info->stackPointer->name);

            try_release_scratch_register(info, scratch);
        }
        break;

        case WB_GLOBAL:
            InternalError("Lifetime for argument %s has global writeback!", argLifetime->name);
            break;

        case WB_UNKNOWN:
            InternalError("Lifetime for argument %s has unknown writeback!", argLifetime->name);
            break;
        }
    }
}

void riscv_generate_internal_copy(struct TACLine *correspondingTACLine,
                                  struct CodegenState *state,
                                  struct Register *sourceAddrReg,
                                  struct Register *destAddrReg,
                                  struct Register *intermediateReg,
                                  size_t moveSize)
{
    size_t offset = 0;
    while (offset < moveSize)
    {
        size_t byteDiff = moveSize - offset;
        if (byteDiff >= sizeof(size_t))
        {
            byteDiff = sizeof(size_t);
        }
        else
        {
            byteDiff = 1;
        }

        char widthChar = riscv_select_width_char_for_size(byteDiff);
        emit_instruction(correspondingTACLine, state, "\tl%c%s %s, %zu(%s)\n",
                         widthChar,
                         riscv_select_sign_for_load_char(widthChar),
                         intermediateReg->name,
                         offset,
                         sourceAddrReg->name);

        emit_instruction(correspondingTACLine, state, "\ts%c %s, %zu(%s)\n",
                         widthChar,
                         intermediateReg->name,
                         offset,
                         destAddrReg->name);
        offset += byteDiff;
    }
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
void riscv_generate_code_for_tac(struct CodegenState *state,
                                 struct RegallocMetadata *metadata,
                                 struct MachineInfo *info,
                                 struct TACLine *generate,
                                 char *functionName,
                                 struct Stack *calledFunctionArguments)
{
    switch (generate->operation)
    {
    case TT_ASM:
        emit_instruction(generate, state, "%s\n", generate->operands[0].name.str);
        break;

    case TT_ASSIGN:
    {
        struct Lifetime *writtenLt = lifetime_find(metadata->allLifetimes, generate->operands[0].name.str);
        if (type_is_object(&writtenLt->type))
        {
            struct Register *sourceAddrReg = acquire_scratch_register(info);
            riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], sourceAddrReg);
            struct Register *destAddrReg = acquire_scratch_register(info);
            riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[0], destAddrReg);

            struct Register *intermediateReg = acquire_scratch_register(info);

            riscv_generate_internal_copy(generate, state, sourceAddrReg, destAddrReg, intermediateReg, type_get_size(&writtenLt->type, metadata->scope));
        }
        else
        {
            // only works for primitive types that will fit in registers
            struct Register *opLocReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
            riscv_write_variable(generate, state, metadata, info, &generate->operands[0], opLocReg);
        }
    }
    break;

    case TT_ADD:
    case TT_SUBTRACT:
    case TT_MUL:
    case TT_DIV:
    case TT_MODULO:
    case TT_BITWISE_AND:
    case TT_BITWISE_OR:
    case TT_BITWISE_XOR:
    case TT_LSHIFT:
    case TT_RSHIFT:
    {
        struct Register *op1Reg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
        struct Register *op2Reg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], acquire_scratch_register(info));

        // release any scratch registers we may have acquired by placing operands, as our write register may be able to use one of them
        try_release_scratch_register(info, op1Reg);
        try_release_scratch_register(info, op2Reg);

        struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));

        emit_instruction(generate, state, "\t%s %s, %s, %s\n", get_asm_op(generate->operation), destReg->name, op1Reg->name, op2Reg->name);
        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
    }
    break;

    case TT_BITWISE_NOT:
    {
        struct Register *op1Reg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
        struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));

        // FIXME: work with new register allocation
        emit_instruction(generate, state, "\txori %s, %s, -1\n", destReg->name, op1Reg->name);
        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
    }
    break;

    case TT_LOAD:
    {
        struct Lifetime *writtenLt = lifetime_find(metadata->allLifetimes, generate->operands[0].name.str);
        if (type_is_object(&writtenLt->type))
        {
            struct Register *sourceAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
            struct Register *destAddrReg = acquire_scratch_register(info);
            riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[0], destAddrReg);

            struct Register *intermediateReg = acquire_scratch_register(info);

            riscv_generate_internal_copy(generate, state, sourceAddrReg, destAddrReg, intermediateReg, type_get_size(&writtenLt->type, metadata->scope));
        }
        else
        {
            struct Register *baseReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
            struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));
            char loadWidth = riscv_select_width_char(metadata->scope, &generate->operands[0]);
            emit_instruction(generate, state, "\tl%c%s %s, 0(%s)\n",
                             loadWidth,
                             riscv_select_sign_for_load_char(loadWidth),
                             destReg->name,
                             baseReg->name);

            riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
        }
    }
    break;

    case TT_LOAD_OFF:
    {
        // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
        struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));
        struct Register *baseReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));

        char loadWidth = riscv_select_width_char(metadata->scope, &generate->operands[0]);
        emit_instruction(generate, state, "\tl%c%s %s, %d(%s)\n",
                         loadWidth,
                         riscv_select_sign_for_load_char(loadWidth),
                         destReg->name,
                         generate->operands[2].name.val,
                         baseReg->name);

        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
    }
    break;

    case TT_LOAD_ARR:
    {
        struct Register *baseReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
        struct Register *offsetReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], acquire_scratch_register(info));

        // because offsetReg may or may not be modifiable, we will immediately release it if it's a temp, and guarantee that shiftedOffsetReg is a temp that we can modify it to
        try_release_scratch_register(info, offsetReg);
        struct Register *shiftedOffsetReg = acquire_scratch_register(info);

        // TODO: check for shift by 0 and don't shift when applicable
        // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
        emit_instruction(generate, state, "\tslli %s, %s, %d\n",
                         shiftedOffsetReg->name,
                         offsetReg->name,
                         generate->operands[3].name.val);

        try_release_scratch_register(info, baseReg);
        try_release_scratch_register(info, shiftedOffsetReg);
        struct Register *addrReg = acquire_scratch_register(info);
        // add our scaled offset to the base address, put the full address into selectScratchRegister(info, false)
        emit_instruction(generate, state, "\tadd %s, %s, %s\n",
                         addrReg->name,
                         baseReg->name,
                         shiftedOffsetReg->name);

        try_release_scratch_register(info, shiftedOffsetReg);
        struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));
        char loadWidth = riscv_select_width_char_for_dereference(metadata->scope, &generate->operands[1]);
        emit_instruction(generate, state, "\tl%c%s %s, 0(%s)\n",
                         loadWidth,
                         riscv_select_sign_for_load_char(loadWidth),
                         destReg->name,
                         addrReg->name);

        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
    }
    break;

    case TT_STORE:
    {
        struct Type *srcType = tac_get_type_of_operand(generate, 1);
        size_t moveSize = type_get_size(srcType, metadata->scope);
        struct Register *destAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[0], acquire_scratch_register(info));
        if (moveSize > sizeof(size_t))
        {
            struct Register *sourceAddrReg = acquire_scratch_register(info);
            riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], sourceAddrReg);
            struct Register *intermediateReg = acquire_scratch_register(info);

            riscv_generate_internal_copy(generate, state, sourceAddrReg, destAddrReg, intermediateReg, moveSize);
        }
        else
        {
            struct Register *sourceReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
            char storeWidth = riscv_select_width_char_for_dereference(metadata->scope, &generate->operands[0]);

            emit_instruction(generate, state, "\ts%c %s, 0(%s)\n",
                             storeWidth,
                             sourceReg->name,
                             destAddrReg->name);
        }
    }
    break;

    case TT_STORE_OFF:
    {
        // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
        struct Register *baseReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[0], acquire_scratch_register(info));
        struct Register *sourceReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], acquire_scratch_register(info));
        emit_instruction(generate, state, "\ts%c %s, %d(%s)\n",
                         riscv_select_width_char(metadata->scope, &generate->operands[0]),
                         sourceReg->name,
                         generate->operands[1].name.val,
                         baseReg->name);
    }
    break;

    case TT_STORE_ARR:
    {
        struct Register *baseReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[0], acquire_scratch_register(info));
        struct Register *offsetReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));

        // TODO: check for shift by 0 and don't shift when applicable
        // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
        emit_instruction(generate, state, "\tslli %s, %s, %d\n",
                         offsetReg->name,
                         offsetReg->name,
                         generate->operands[2].name.val);

        try_release_scratch_register(info, baseReg);
        struct Register *addrReg = acquire_scratch_register(info);
        // add our scaled offset to the base address, put the full address into selectScratchRegister(info, false)
        emit_instruction(generate, state, "\tadd %s, %s, %s\n",
                         addrReg->name,
                         baseReg->name,
                         offsetReg->name);

        try_release_scratch_register(info, offsetReg);
        struct Register *sourceReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[3], acquire_scratch_register(info));
        emit_instruction(generate, state, "\ts%c %s, 0(%s)\n",
                         riscv_select_width_char_for_dereference(metadata->scope, &generate->operands[0]),
                         sourceReg->name,
                         addrReg->name);
    }
    break;

    case TT_ADDROF:
    {
        struct Register *addrReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], addrReg);
        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], addrReg);
    }
    break;

    case TT_LEA_OFF:
    {
        // TODO: need to switch for when immediate values exceed the 12-bit size permitted in immediate instructions
        struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));
        struct Register *baseReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));

        emit_instruction(generate, state, "\taddi %s, %s, %d\n",
                         destReg->name,
                         baseReg->name,
                         generate->operands[2].name.val);

        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
    }
    break;

    case TT_LEA_ARR:
    {
        struct Register *baseReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
        struct Register *offsetReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], acquire_scratch_register(info));

        // because offsetReg may or may not be modifiable, we will immediately release it if it's a temp, and guarantee that shiftedOffsetReg is a temp that we can modify it to
        try_release_scratch_register(info, offsetReg);
        struct Register *shiftedOffsetReg = acquire_scratch_register(info);

        // TODO: check for shift by 0 and don't shift when applicable
        // perform a left shift by however many bits necessary to scale our value, place the result in selectScratchRegister(info, false)
        emit_instruction(generate, state, "\tslli %s, %s, %d\n",
                         shiftedOffsetReg->name,
                         offsetReg->name,
                         generate->operands[3].name.val);

        // release any scratch registers we may have acquired by placing operands, as our write register may be able to use one of them
        release_all_scratch_registers(info);
        struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));
        // add our scaled offset to the base address, put the full address into destReg
        emit_instruction(generate, state, "\tadd %s, %s, %s\n",
                         destReg->name,
                         baseReg->name,
                         shiftedOffsetReg->name);

        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
    }
    break;

    case TT_BEQ:
    case TT_BNE:
    case TT_BGEU:
    case TT_BLTU:
    case TT_BGTU:
    case TT_BLEU:
    case TT_BEQZ:
    case TT_BNEZ:
    {
        struct Register *operand1register = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], acquire_scratch_register(info));
        struct Register *operand2register = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], acquire_scratch_register(info));
        emit_instruction(generate, state, "\t%s %s, %s, %s_%d\n",
                         get_asm_op(generate->operation),
                         operand1register->name,
                         operand2register->name,
                         functionName,
                         generate->operands[0].name.val);
    }
    break;

    case TT_JMP:
    {
        emit_instruction(generate, state, "\tj %s_%d\n", functionName, generate->operands[0].name.val);
    }
    break;

    case TT_ARG_STORE:
    {
        stack_push(calledFunctionArguments, &generate->operands[0]);
    }
    break;

    case TT_FUNCTION_CALL:
    {
        struct FunctionEntry *calledFunction = lookup_fun_by_string(metadata->function->mainScope, generate->operands[1].name.str);

        riscv_caller_save_registers(state, calledFunction, info);

        riscv_emit_argument_stores(state, metadata, info, calledFunction, calledFunctionArguments);
        calledFunctionArguments->size = 0;

        if (calledFunction->isDefined)
        {
            emit_instruction(generate, state, "\tcall %s\n", generate->operands[1].name.str);
        }
        else
        {
            emit_instruction(generate, state, "\tcall %s@plt\n", generate->operands[1].name.str);
        }

        if ((generate->operands[0].name.str != NULL) && !type_is_object(&calledFunction->returnType))
        {
            riscv_write_variable(generate, state, metadata, info, &generate->operands[0], info->returnValue);
        }

        riscv_caller_restore_registers(state, calledFunction, info);
    }
    break;

    case TT_METHOD_CALL:
    {
        struct StructEntry *methodOf = scope_lookup_struct_by_type(metadata->scope, tac_get_type_of_operand(generate, 2));
        struct FunctionEntry *calledMethod = struct_lookup_method_by_string(methodOf, generate->operands[1].name.str);

        riscv_caller_save_registers(state, calledMethod, info);

        riscv_emit_argument_stores(state, metadata, info, calledMethod, calledFunctionArguments);
        calledFunctionArguments->size = 0;

        // TODO: member function name mangling/uniqueness
        if (calledMethod->isDefined)
        {
            emit_instruction(generate, state, "\tcall %s_%s\n", methodOf->name, generate->operands[1].name.str);
        }
        else
        {
            emit_instruction(generate, state, "\tcall %s_%s@plt\n", methodOf->name, generate->operands[1].name.str);
        }

        if ((generate->operands[0].name.str != NULL) && !type_is_object(&calledMethod->returnType))
        {
            riscv_write_variable(generate, state, metadata, info, &generate->operands[0], info->returnValue);
        }

        riscv_caller_restore_registers(state, calledMethod, info);
    }
    break;

    case TT_ASSOCIATED_CALL:
    {
        struct StructEntry *associatedWith = scope_lookup_struct_by_type(metadata->scope, tac_get_type_of_operand(generate, 2));
        struct FunctionEntry *calledAssociated = struct_lookup_method_by_string(associatedWith, generate->operands[1].name.str);

        riscv_caller_save_registers(state, calledAssociated, info);

        riscv_emit_argument_stores(state, metadata, info, calledAssociated, calledFunctionArguments);
        calledFunctionArguments->size = 0;

        // TODO: associated function name mangling/uniqueness
        if (calledAssociated->isDefined)
        {
            emit_instruction(generate, state, "\tcall %s_%s\n", associatedWith->name, generate->operands[1].name.str);
        }
        else
        {
            emit_instruction(generate, state, "\tcall %s_%s@plt\n", associatedWith->name, generate->operands[1].name.str);
        }

        if ((generate->operands[0].name.str != NULL) && !type_is_object(&calledAssociated->returnType))
        {
            riscv_write_variable(generate, state, metadata, info, &generate->operands[0], info->returnValue);
        }

        riscv_caller_restore_registers(state, calledAssociated, info);
    }
    break;

    case TT_LABEL:
        fprintf(state->outFile, "\t%s_%ld:\n", functionName, generate->operands[0].name.val);
        break;

    case TT_RETURN:
    {
        if (generate->operands[0].name.str != NULL)
        {
            if (!(type_is_object(&metadata->function->returnType)))
            {
                struct Register *sourceReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[0], info->returnValue);

                if (sourceReg != info->returnValue)
                {
                    emit_instruction(generate, state, "\tmv %s, %s\n",
                                     info->returnValue->name,
                                     sourceReg->name);
                }
            }
        }
        emit_instruction(generate, state, "\tj %s_done\n", functionName);
    }
    break;

    case TT_DO:
    case TT_ENDDO:
    case TT_PHI:
        break;
    }
}
// NOLINTEND(readability-function-cognitive-complexity)

void riscv_generate_code_for_basic_block(struct CodegenState *state,
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

    struct Stack *calledFunctionArguments = stack_new();
    size_t lastLineNo = 0;
    for (struct LinkedListNode *tacRunner = block->TACList->head; tacRunner != NULL; tacRunner = tacRunner->next)
    {
        release_all_scratch_registers(info);

        struct TACLine *thisTac = tacRunner->data;

        char *printedTac = sprint_tac_line(thisTac);
        log(LOG_DEBUG, "Generate code for %s (alloc %s:%d)", printedTac, thisTac->allocFile, thisTac->allocLine);
        fprintf(state->outFile, "#%s\n", printedTac);
        free(printedTac);

        emit_loc(state, thisTac, &lastLineNo);
        riscv_generate_code_for_tac(state, metadata, info, thisTac, functionName, calledFunctionArguments);
    }

    stack_free(calledFunctionArguments);
}
