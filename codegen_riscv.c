#include "codegen_riscv.h"

#include "codegen_generic.h"
#include "log.h"
#include "symtab.h"
#include "tac.h"

#include "mbcl/set.h"
#include "mbcl/stack.h"

char *riscv_get_asm_op(enum TAC_TYPE operation)
{
    switch (operation)
    {
    case TT_ADD:
        return "add";
    case TT_SUBTRACT:
        return "sub";
    case TT_MUL:
        return "mul";
    case TT_DIV:
        return "div";
    case TT_MODULO:
        return "rem";
    case TT_BITWISE_AND:
        return "and";
    case TT_BITWISE_OR:
        return "or";
    case TT_BITWISE_XOR:
        return "xor";
    case TT_LSHIFT:
        return "sll";
    case TT_RSHIFT:
        return "srl";
    default:
        InternalError("Lookup for non asm instruction tac type %s in riscv_get_asm_op", tac_operation_get_name(operation));
    }
}

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

Stack *get_touched_caller_save_registers(struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    Stack *actuallyCallerSaved = stack_new(NULL);

    for (size_t regIndex = 0; regIndex < info->caller_save.size; regIndex++)
    {
        struct Register *potentiallyCallerSaved = array_at(&info->caller_save, regIndex);
        // only need to actually callee-save registers we touch in this function
        if (set_find(metadata->touchedRegisters, potentiallyCallerSaved) != NULL)
        {
            stack_push(actuallyCallerSaved, potentiallyCallerSaved);
            log(LOG_DEBUG, "%s is used in %s, need to caller-save", potentiallyCallerSaved->name, metadata->function->name);
        }
    }

    return actuallyCallerSaved;
}

// returns a set of lifetimes for all of the function's arguments which were callee-saved to somewhere on the stack
// this set should be searched before regalloc->allLifetimes so that variables which lived in argument registers but have been stomped can still be read
// this comes in to play when function a() calls function b(), but we want to pass one of a's arguments to b by reading a register which already has been overwritten with one of b's arguments
Set *riscv_caller_save_registers(struct CodegenState *state, struct RegallocMetadata *regalloc, struct MachineInfo *info)
{
    Set *stompedArgLifetimeLocations = set_new(free, (MBCL_DATA_COMPARE_FUNCTION)lifetime_compare);
    log(LOG_DEBUG, "Caller-saving registers");

    Stack *actuallyCallerSaved = get_touched_caller_save_registers(regalloc, info);

    if (actuallyCallerSaved->size == 0)
    {
        stack_free(actuallyCallerSaved);
        return stompedArgLifetimeLocations;
    }

    emit_instruction(NULL, state, "\t#Caller-save %zu registers\n", actuallyCallerSaved->size);

    char *spName = info->stackPointer->name;

    // TODO: don't emit when 0
    emit_instruction(NULL, state, "\taddi %s, %s, -%zd\n", spName, spName, MACHINE_REGISTER_SIZE_BYTES * actuallyCallerSaved->size);

    ssize_t saveIndex = 0;
    while (actuallyCallerSaved->size > 0)
    {
        struct Register *callerSaved = stack_pop(actuallyCallerSaved);
        riscv_emit_stack_store_for_size(NULL, state, info, callerSaved, MACHINE_REGISTER_SIZE_BYTES, saveIndex * MACHINE_REGISTER_SIZE_BYTES);
        struct Lifetime *stompedLifetime = callerSaved->containedLifetime;
        if ((stompedLifetime != NULL) && (stompedLifetime->isArgument))
        {
            struct Lifetime *dummyLifetime = malloc(sizeof(struct Lifetime));
            *dummyLifetime = *stompedLifetime;
            dummyLifetime->wbLocation = WB_STACK;
            dummyLifetime->writebackInfo.stackOffset = saveIndex * MACHINE_REGISTER_SIZE_BYTES;
            log(LOG_DEBUG, "Create temporary dummy lifetime for %s as it was in an argument register which is being caller-saved", dummyLifetime->name);
            set_insert(stompedArgLifetimeLocations, dummyLifetime);
        }

        saveIndex++;
    }

    stack_free(actuallyCallerSaved);

    return stompedArgLifetimeLocations;
}

void riscv_caller_restore_registers(struct CodegenState *state, struct RegallocMetadata *regalloc, struct MachineInfo *info)
{
    log(LOG_DEBUG, "Caller-restoring registers");

    Stack *actuallyCallerSaved = get_touched_caller_save_registers(regalloc, info);

    if (actuallyCallerSaved->size == 0)
    {
        stack_free(actuallyCallerSaved);
        return;
    }

    emit_instruction(NULL, state, "\t#Caller-restore registers\n");

    char *spName = info->stackPointer->name;
    ssize_t saveIndex = 0;
    while (actuallyCallerSaved->size > 0)
    {
        struct Register *callerSaved = stack_pop(actuallyCallerSaved);
        riscv_emit_stack_load_for_size(NULL, state, info, callerSaved, MACHINE_REGISTER_SIZE_BYTES, (saveIndex * MACHINE_REGISTER_SIZE_BYTES));
        saveIndex++;
    }

    // TODO: don't emit when 0
    emit_instruction(NULL, state, "\taddi %s, %s, %zd\n", spName, spName, MACHINE_REGISTER_SIZE_BYTES * saveIndex);

    stack_free(actuallyCallerSaved);
}

Stack *get_touched_callee_save_registers(struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    Stack *actuallyCalleeSaved = stack_new(NULL);

    for (size_t regIndex = 0; regIndex < info->callee_save.size; regIndex++)
    {
        struct Register *potentiallyCalleeSaved = array_at(&info->callee_save, regIndex);
        // only need to actually callee-save registers we touch in this function
        if (set_find(metadata->touchedRegisters, potentiallyCalleeSaved) != NULL)
        {
            stack_push(actuallyCalleeSaved, potentiallyCalleeSaved);
            log(LOG_DEBUG, "%s is used in %s, need to callee-save", potentiallyCalleeSaved->name, metadata->function->name);
        }
    }

    return actuallyCalleeSaved;
}

void riscv_callee_save_registers(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    log(LOG_DEBUG, "Callee-saving registers");
    Stack *actuallyCalleeSaved = get_touched_callee_save_registers(metadata, info);

    if (actuallyCalleeSaved->size == 0)
    {
        stack_free(actuallyCalleeSaved);
        return;
    }

    emit_instruction(NULL, state, "\t#Callee-save %zu registers\n", actuallyCalleeSaved->size);

    ssize_t saveIndex = 0;
    while (actuallyCalleeSaved->size > 0)
    {
        struct Register *calleeSaved = stack_pop(actuallyCalleeSaved);
        riscv_emit_frame_store_for_size(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (saveIndex + 2) * MACHINE_REGISTER_SIZE_BYTES));
        saveIndex++;
    }

    stack_free(actuallyCalleeSaved);
}

void riscv_callee_restore_registers(struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    log(LOG_DEBUG, "Callee-restoring registers");
    Stack *actuallyCalleeSaved = get_touched_callee_save_registers(metadata, info);

    if (actuallyCalleeSaved->size == 0)
    {
        stack_free(actuallyCalleeSaved);
        return;
    }

    emit_instruction(NULL, state, "\t#Callee-restore %zu registers\n", actuallyCalleeSaved->size);

    ssize_t saveIndex = 0;
    while (actuallyCalleeSaved->size > 0)
    {
        struct Register *calleeSaved = stack_pop(actuallyCalleeSaved);
        // +2, 1 to account for stack growing downward and 1 to account for saved frame pointer
        riscv_emit_frame_load_for_size(NULL, state, info, calleeSaved, MACHINE_REGISTER_SIZE_BYTES, (-1 * (saveIndex + 2) * MACHINE_REGISTER_SIZE_BYTES));
        saveIndex++;
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

    emit_instruction(NULL, state, "\taddi %s, %s, %zu # free up local stack\n", info->stackPointer->name, info->stackPointer->name, metadata->localStackSize);

    riscv_emit_frame_load_for_size(NULL, state, info, info->framePointer, MACHINE_REGISTER_SIZE_BYTES, ((ssize_t)-1 * MACHINE_REGISTER_SIZE_BYTES));

    // TODO: don't emit when 0
    emit_instruction(NULL, state, "\taddi %s, %s, %zd # free up argument stack\n", info->stackPointer->name, info->stackPointer->name, metadata->argStackSize);

    emit_instruction(NULL, state, "\tjalr zero, 0(%s)\n", info->returnAddress->name);
    fprintf(state->outFile, "\t.cfi_endproc\n");
}

struct Register *register_use_optional_scratch_or_acquire(struct MachineInfo *info, struct Register *optionalScratch)
{
    struct Register *used = optionalScratch;
    if (used == NULL)
    {
        used = acquire_scratch_register(info);
    }

    return used;
}

// Returns the index of the register containing operand if it's has a register writeback location
// If not a register writeback location, and optionalScratch is non-null, it will place the operand in optionalScratch
// if optionalScratch is null, a new scratch register will be acquired and the operand will be placed there
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
struct Register *riscv_place_or_find_operand_in_register(struct TACLine *correspondingTACLine,
                                                         struct CodegenState *state,
                                                         struct RegallocMetadata *metadata,
                                                         struct MachineInfo *info,
                                                         struct TACOperand *operand,
                                                         struct Register *optionalScratch)
{
    verify_codegen_primitive(operand);

    struct Register *placedOrFoundIn = NULL;
    if (operand->permutation == VP_LITERAL_STR)
    {
        placedOrFoundIn = register_use_optional_scratch_or_acquire(info, optionalScratch);
        riscv_place_literal_string_in_register(correspondingTACLine, state, operand->name.str, placedOrFoundIn);
        return placedOrFoundIn;
    }

    if (operand->permutation == VP_LITERAL_VAL)
    {
        placedOrFoundIn = register_use_optional_scratch_or_acquire(info, optionalScratch);
        riscv_place_literal_value_in_register(correspondingTACLine, state, operand->name.val, placedOrFoundIn);
        return placedOrFoundIn;
    }

    struct Lifetime *operandLt = lifetime_find(metadata->allLifetimes, operand->name.variable->name);
    if (operandLt == NULL)
    {
        InternalError("Unable to find lifetime for variable %s!", operand->name.variable->name);
    }

    switch (operandLt->wbLocation)
    {
    case WB_REGISTER:
        placedOrFoundIn = operandLt->writebackInfo.regLocation;
        break;

    case WB_STACK:
        placedOrFoundIn = register_use_optional_scratch_or_acquire(info, optionalScratch);
        if (type_is_object(tac_operand_get_type(operand)))
        {
            if (operandLt->writebackInfo.stackOffset >= 0)
            {
                emit_instruction(correspondingTACLine, state, "\taddi %s, fp, %d # place %s\n", placedOrFoundIn->name, operandLt->writebackInfo.stackOffset, operand->name.str);
            }
            else
            {
                emit_instruction(correspondingTACLine, state, "\taddi %s, fp, -%d # place %s\n", placedOrFoundIn->name, -1 * operandLt->writebackInfo.stackOffset, operand->name.str);
            }
        }
        else
        {
            riscv_emit_frame_load_for_size(correspondingTACLine, state, info, placedOrFoundIn, type_get_size(tac_operand_get_type(operand), metadata->scope), operandLt->writebackInfo.stackOffset);
        }
        break;

    case WB_GLOBAL:
    {
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

        placedOrFoundIn = register_use_optional_scratch_or_acquire(info, optionalScratch);
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
    switch (writtenTo->permutation)
    {
    case VP_STANDARD:
    case VP_TEMP:
        break;

    default:
        InternalError("TAC Operand with non standard/temp permutation passed to riscv_write_variable!");
    }

    struct Lifetime *writtenLifetime = lifetime_find(metadata->allLifetimes, writtenTo->name.variable->name);
    if (writtenLifetime == NULL)
    {
        InternalError("No lifetime found for %s", writtenTo->name.variable->name);
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

void riscv_place_literal_value_in_register(struct TACLine *correspondingTACLine,
                                           struct CodegenState *state,
                                           size_t literalVal,
                                           struct Register *destReg)
{
    emit_instruction(correspondingTACLine, state, "\tli %s, 0x%lx # place literal\n", destReg->name, literalVal);
}

void riscv_place_addr_of_operand_in_reg(struct TACLine *correspondingTACLine,
                                        struct CodegenState *state,
                                        struct RegallocMetadata *metadata,
                                        struct MachineInfo *info,
                                        struct TACOperand *operand,
                                        struct Register *destReg)
{
    struct Lifetime *lifetime = lifetime_find(metadata->allLifetimes, operand->name.variable->name);
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
void riscv_emit_argument_stores(struct CodegenState *state,
                                struct RegallocMetadata *metadata,
                                struct MachineInfo *info,
                                struct FunctionEntry *calledFunction,
                                Stack *argumentOperands,
                                Set *callerSavedArgLifetimes)
{
    log(LOG_DEBUG, "Emit argument stores for call to %s", calledFunction->name);
    // TODO: don't emit when 0
    emit_instruction(NULL, state, "\taddi %s, %s, -%zd\n", info->stackPointer->name, info->stackPointer->name, calledFunction->regalloc.argStackSize);

    Iterator *argumentIterator = deque_rear(calledFunction->arguments);

    Set *stompedArgRegs = set_new(NULL, register_compare);
    // problem: when function a() calls function b(), if we are copying one of a's arguments to one of b's arguments it is possible that we will try to load one of a's arguments from a register which has been overwritten with one of b's arguments already.
    while (argumentOperands->size > 0)
    {
        struct TACOperand *argOperand = stack_pop(argumentOperands);

        struct VariableEntry *argument = iterator_get(argumentIterator);

        if (type_compare_allow_implicit_widening(tac_operand_get_type(argOperand), &argument->type))
        {
            InternalError("Type mismatch during internal argument store handling for argument %s of function %s", argument->name, calledFunction->name);
        }

        iterator_prev(argumentIterator);

        struct Lifetime *argLifetime = lifetime_find(calledFunction->regalloc.allLifetimes, argument->name);

        log(LOG_DEBUG, "Store argument %s - %s", argument->name, argOperand->name.str);
        char *printedOperand = tac_operand_sprint(argOperand);
        emit_instruction(NULL, state, "\t#Store argument %s - %s\n", argument->name, printedOperand);
        free(printedOperand);
        switch (argLifetime->wbLocation)
        {
        case WB_REGISTER:
        {
            struct Register *destinationArgRegister = argLifetime->writebackInfo.regLocation;

            struct Register *placedOrFoundIn = NULL;
            switch (argOperand->permutation)
            {
            case VP_STANDARD:
            case VP_TEMP:
            {
                struct Lifetime *callerSavedDummyLifetime = lifetime_find(callerSavedArgLifetimes, argOperand->name.variable->name);

                if (callerSavedDummyLifetime != NULL)
                {
                    placedOrFoundIn = acquire_scratch_register(info);
                    riscv_emit_stack_load_for_size(NULL, state, info, placedOrFoundIn, type_get_size(tac_operand_get_type(argOperand), metadata->scope), callerSavedDummyLifetime->writebackInfo.stackOffset);
                }
                else
                {
                    placedOrFoundIn = riscv_place_or_find_operand_in_register(NULL, state, metadata, info, argOperand, destinationArgRegister);
                }

                struct Register *attemptedStompedAccess = set_find(stompedArgRegs, placedOrFoundIn);
                if (attemptedStompedAccess != NULL)
                {
                    InternalError("When attempting to store argument %s for call to %s - the value we want to read from (%s) is contained in %s, an argument register we've already overwritten with one of %s's arguments",
                                  argLifetime->name, calledFunction->name, argOperand->name.variable->name, placedOrFoundIn->name, calledFunction->name);
                }
            }
            break;

            case VP_LITERAL_STR:
            case VP_LITERAL_VAL:
            case VP_UNUSED:
                placedOrFoundIn = riscv_place_or_find_operand_in_register(NULL, state, metadata, info, argOperand, destinationArgRegister);
                break;
            }

            if (destinationArgRegister != placedOrFoundIn)
            {
                emit_instruction(NULL, state, "\tmv %s, %s\n", destinationArgRegister->name, placedOrFoundIn->name);
            }
            else
            {
                emit_instruction(NULL, state, "\t# already in %s\n", placedOrFoundIn->name);
            }
        }
            set_insert(stompedArgRegs, argLifetime->writebackInfo.regLocation);
            break;

        case WB_STACK:
        {
            struct Register *scratch = acquire_scratch_register(info);
            if (type_is_object(tac_operand_get_type(argOperand)))
            {
                struct Register *sourceAddrReg = acquire_scratch_register(info);
                riscv_place_addr_of_operand_in_reg(NULL, state, metadata, info, argOperand, sourceAddrReg);

                struct Register *destAddrReg = acquire_scratch_register(info);
                emit_instruction(NULL, state, "\t#compute pointer to stack argument %s to store %s\n", argument->name, argLifetime->name);
                emit_instruction(NULL, state, "\taddi %s, %s, %zd\n", destAddrReg->name, info->stackPointer->name, argLifetime->writebackInfo.stackOffset);

                riscv_generate_internal_copy(NULL, state, sourceAddrReg, destAddrReg, scratch, type_get_size(tac_operand_get_type(argOperand), metadata->scope));
            }
            else
            {
                struct Register *writeFrom = riscv_place_or_find_operand_in_register(NULL, state, metadata, info, argOperand, scratch);

                emit_instruction(NULL, state, "\ts%c %s, %zd(%s)\n",
                                 riscv_select_width_char_for_lifetime(calledFunction->mainScope, argLifetime),
                                 writeFrom->name,
                                 argLifetime->writebackInfo.stackOffset,
                                 info->stackPointer->name);
            }

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

    set_free(stompedArgRegs);

    iterator_free(argumentIterator);
}
// NOLINTEND(readability-function-cognitive-complexity)

void riscv_emit_array_load(struct TACLine *generate, struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    if (generate->operation != TT_ARRAY_LOAD)
    {
        InternalError("Incorrect TAC type %s passed to riscv_emit_array_load", tac_operation_get_name(generate->operation));
    }

    struct Lifetime *loadedFromLt = lifetime_find(metadata->allLifetimes, generate->operands[1].name.variable->name);
    switch (loadedFromLt->wbLocation)
    {
    case WB_STACK:
    case WB_GLOBAL:
        break;

    case WB_REGISTER:
        if (type_is_struct_object(&loadedFromLt->type))
        {
            InternalError("Codegen for array load for array with WB_REGISTER not implemented");
        }
        break;

    case WB_UNKNOWN:
        InternalError("Unknown writeback location for lifetime %s (%s)", loadedFromLt->name, type_get_name(&loadedFromLt->type));
    }

    struct Type *loadedFromArrayType = tac_get_type_of_operand(generate, 1);
    struct Type *loadedType = tac_get_type_of_operand(generate, 0); // the type of the thing actually being loaded (for load size)

    struct Register *arrayIndexReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], NULL);
    struct Register *scaledIndexReg = acquire_scratch_register(info);
    emit_instruction(generate, state, "\tli %s, %zu\n", scaledIndexReg->name, type_get_size_of_array_element(loadedFromArrayType, metadata->scope));
    emit_instruction(generate, state, "\tmul %s, %s, %s\n", scaledIndexReg->name, arrayIndexReg->name, scaledIndexReg->name);
    try_release_scratch_register(info, arrayIndexReg);

    // TODO: this really supports array index operations on arrays and array single pointers. Ensure that array single pointers are []'d correctly (linearization issue? if an issue at all)
    struct Register *arrayBaseAddrReg = NULL;
    if (type_is_struct_object(&loadedFromLt->type))
    {
        arrayBaseAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], arrayBaseAddrReg);
    }
    else
    {
        arrayBaseAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
    }

    try_release_scratch_register(info, arrayBaseAddrReg);
    try_release_scratch_register(info, scaledIndexReg);
    struct Register *computedAddressRegister = acquire_scratch_register(info);
    emit_instruction(generate, state, "\tadd %s, %s, %s\n", computedAddressRegister->name, arrayBaseAddrReg->name, scaledIndexReg->name);

    if (!type_is_object(loadedType))
    {
        struct Register *loadedTo = acquire_scratch_register(info);
        char loadChar = riscv_select_width_char_for_size(type_get_size(loadedType, metadata->scope));
        emit_instruction(generate, state, "\tl%c%s %s, 0(%s)\n",
                         loadChar,
                         riscv_select_sign_for_load_char(loadChar),
                         loadedTo->name,
                         computedAddressRegister->name);
        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], loadedTo);
    }
    else
    {
        struct Register *destAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[0], destAddrReg);
        riscv_generate_internal_copy(generate, state, computedAddressRegister, destAddrReg, acquire_scratch_register(info), type_get_size(loadedType, metadata->scope));
    }
}

void riscv_emit_array_lea(struct TACLine *generate, struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    if (generate->operation != TT_ARRAY_LEA)
    {
        InternalError("Incorrect TAC type %s passed to riscv_emit_array_lea", tac_operation_get_name(generate->operation));
    }

    struct Lifetime *loadedFromLt = lifetime_find(metadata->allLifetimes, generate->operands[1].name.variable->name);
    switch (loadedFromLt->wbLocation)
    {
    case WB_STACK:
    case WB_GLOBAL:
        break;

    case WB_REGISTER:
        if (type_is_struct_object(&loadedFromLt->type))
        {
            InternalError("Codegen for array load for array with WB_REGISTER not implemented");
        }
        break;

    case WB_UNKNOWN:
        InternalError("Unknown writeback location for lifetime %s (%s)", loadedFromLt->name, type_get_name(&loadedFromLt->type));
    }

    struct Type *loadedFromArrayType = tac_get_type_of_operand(generate, 1);

    struct Register *arrayIndexReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], NULL);
    struct Register *scaledIndexReg = acquire_scratch_register(info);
    emit_instruction(generate, state, "\tli %s, %zu\n", scaledIndexReg->name, type_get_size_of_array_element(loadedFromArrayType, metadata->scope));
    emit_instruction(generate, state, "\tmul %s, %s, %s\n", scaledIndexReg->name, arrayIndexReg->name, scaledIndexReg->name);
    try_release_scratch_register(info, arrayIndexReg);

    // TODO: this really supports array index operations on arrays and array single pointers. Ensure that array single pointers are []'d correctly (linearization issue? if an issue at all)
    struct Register *arrayBaseAddrReg = NULL;
    if (type_is_struct_object(&loadedFromLt->type))
    {
        arrayBaseAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], arrayBaseAddrReg);
    }
    else
    {
        arrayBaseAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
    }

    try_release_scratch_register(info, arrayBaseAddrReg);
    try_release_scratch_register(info, scaledIndexReg);
    struct Register *computedAddressRegister = acquire_scratch_register(info);
    emit_instruction(generate, state, "\tadd %s, %s, %s\n", computedAddressRegister->name, arrayBaseAddrReg->name, scaledIndexReg->name);

    riscv_write_variable(generate, state, metadata, info, &generate->operands[0], computedAddressRegister);
}

void riscv_emit_array_store(struct TACLine *generate, struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    if (generate->operation != TT_ARRAY_STORE)
    {
        InternalError("Incorrect TAC type %s passed to riscv_emit_array_store", tac_operation_get_name(generate->operation));
    }

    struct Lifetime *storedToLt = lifetime_find(metadata->allLifetimes, generate->operands[0].name.variable->name);
    switch (storedToLt->wbLocation)
    {
    case WB_STACK:
    case WB_GLOBAL:
        break;

    case WB_REGISTER:
        if (type_is_struct_object(&storedToLt->type))
        {
            InternalError("Codegen for array load for array with WB_REGISTER not implemented");
        }
        break;

    case WB_UNKNOWN:
        InternalError("Unknown writeback location for lifetime %s (%s)", storedToLt->name, type_get_name(&storedToLt->type));
    }

    struct Type *storedToArrayType = tac_get_type_of_operand(generate, 0); // what the original type of the array is (for offset computation)
    struct Type *storedType = tac_get_type_of_operand(generate, 2);        // the type of the thing actually being loaded (for load size)

    struct Register *arrayIndexReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
    struct Register *scaledIndexReg = acquire_scratch_register(info);
    emit_instruction(generate, state, "\tli %s, %zu\n", scaledIndexReg->name, type_get_size_of_array_element(storedToArrayType, metadata->scope));
    emit_instruction(generate, state, "\tmul %s, %s, %s\n", scaledIndexReg->name, arrayIndexReg->name, scaledIndexReg->name);
    try_release_scratch_register(info, arrayIndexReg);

    // TODO: this really supports array index operations on arrays and array single pointers. Ensure that array single pointers are []'d correctly (linearization issue? if an issue at all)
    struct Register *arrayBaseAddrReg = NULL;
    if (type_is_struct_object(&storedToLt->type))
    {
        arrayBaseAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[0], arrayBaseAddrReg);
    }
    else
    {
        arrayBaseAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[0], NULL);
    }

    try_release_scratch_register(info, scaledIndexReg);
    try_release_scratch_register(info, arrayBaseAddrReg);
    struct Register *computedAddressRegister = acquire_scratch_register(info);
    emit_instruction(generate, state, "\tadd %s, %s, %s\n", computedAddressRegister->name, arrayBaseAddrReg->name, scaledIndexReg->name);

    if (!type_is_object(storedType))
    {
        struct Register *storedFrom = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], NULL);
        char storeChar = riscv_select_width_char_for_size(type_get_size(storedType, metadata->scope));
        emit_instruction(generate, state, "\ts%c %s, 0(%s)\n",
                         storeChar,
                         storedFrom->name,
                         computedAddressRegister->name);
    }
    else
    {
        struct Register *sourceAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[2], sourceAddrReg);
        riscv_generate_internal_copy(generate, state, sourceAddrReg, computedAddressRegister, acquire_scratch_register(info), type_get_size(storedType, metadata->scope));
    }
}

void riscv_emit_struct_field_load(struct TACLine *generate, struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    if (generate->operation != TT_FIELD_LOAD)
    {
        InternalError("Incorrect TAC type %s passed to riscv_emit_struct_field_load", tac_operation_get_name(generate->operation));
    }

    struct Lifetime *loadedFromLt = lifetime_find(metadata->allLifetimes, generate->operands[1].name.variable->name);
    switch (loadedFromLt->wbLocation)
    {
    case WB_STACK:
    case WB_GLOBAL:
        break;

    case WB_REGISTER:
        if (type_is_struct_object(&loadedFromLt->type))
        {
            InternalError("Codegen for struct field load for struct with WB_REGISTER not implemented");
        }
        break;

    case WB_UNKNOWN:
        InternalError("Unknown writeback location for lifetime %s (%s)", loadedFromLt->name, type_get_name(&loadedFromLt->type));
    }

    struct StructEntry *loadedFromStruct = scope_lookup_struct_by_type(metadata->scope, &loadedFromLt->type);
    struct StructField *loadedField = struct_lookup_field_by_name(loadedFromStruct, generate->operands[2].name.str, metadata->scope);

    struct Register *structBaseAddrReg = NULL;
    if (type_is_struct_object(&loadedFromLt->type))
    {
        structBaseAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], structBaseAddrReg);
    }
    else
    {
        structBaseAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
    }

    if (!type_is_object(&loadedField->variable->type))
    {
        struct Register *loadedTo = acquire_scratch_register(info);
        char loadChar = riscv_select_width_char_for_size(type_get_size(&loadedField->variable->type, metadata->scope));
        emit_instruction(generate, state, "\tl%c%s %s, %zd(%s)\n",
                         loadChar,
                         riscv_select_sign_for_load_char(loadChar),
                         loadedTo->name,
                         loadedField->offset,
                         structBaseAddrReg->name);
        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], loadedTo);
    }
    else
    {
        struct Register *destAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[0], destAddrReg);
        riscv_generate_internal_copy(generate, state, structBaseAddrReg, destAddrReg, acquire_scratch_register(info), type_get_size(&loadedField->variable->type, metadata->scope));
    }
}

void riscv_emit_struct_field_lea(struct TACLine *generate, struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    if (generate->operation != TT_FIELD_LEA)
    {
        InternalError("Incorrect TAC type %s passed to riscv_emit_struct_field_lea", tac_operation_get_name(generate->operation));
    }

    struct Lifetime *loadedFromLt = lifetime_find(metadata->allLifetimes, generate->operands[1].name.variable->name);
    switch (loadedFromLt->wbLocation)
    {
    case WB_STACK:
    case WB_GLOBAL:
        break;

    case WB_REGISTER:
        if (type_is_struct_object(&loadedFromLt->type))
        {
            InternalError("Codegen for struct field lea for struct with WB_REGISTER not implemented");
        }
        break;

    case WB_UNKNOWN:
        InternalError("Unknown writeback location for lifetime %s (%s)", loadedFromLt->name, type_get_name(&loadedFromLt->type));
    }

    struct StructEntry *loadedFromStruct = scope_lookup_struct_by_type(metadata->scope, &loadedFromLt->type);
    struct StructField *loadedField = struct_lookup_field_by_name(loadedFromStruct, generate->operands[2].name.str, metadata->scope);

    struct Register *structBaseAddrReg = NULL;
    if (type_is_struct_object(&loadedFromLt->type))
    {
        structBaseAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], structBaseAddrReg);
    }
    else
    {
        structBaseAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
    }

    struct Register *computedAddressReg = acquire_scratch_register(info);
    emit_instruction(generate, state, "\taddi %s, %s, %zd\n",
                     computedAddressReg->name,
                     structBaseAddrReg->name,
                     loadedField->offset);

    riscv_write_variable(generate, state, metadata, info, &generate->operands[0], computedAddressReg);
}

void riscv_emit_struct_field_store(struct TACLine *generate, struct CodegenState *state, struct RegallocMetadata *metadata, struct MachineInfo *info)
{
    if (generate->operation != TT_FIELD_STORE)
    {
        InternalError("Incorrect TAC type %s passed to riscv_emit_struct_field_store", tac_operation_get_name(generate->operation));
    }

    struct Lifetime *storedToLt = lifetime_find(metadata->allLifetimes, generate->operands[0].name.variable->name);
    switch (storedToLt->wbLocation)
    {
    case WB_STACK:
    case WB_GLOBAL:
        break;

    case WB_REGISTER:
        if (type_is_struct_object(&storedToLt->type))
        {
            InternalError("Codegen for struct field store for struct with WB_REGISTER not implemented");
        }
        break;

    case WB_UNKNOWN:
        InternalError("Unknown writeback location for lifetime %s (%s)", storedToLt->name, type_get_name(&storedToLt->type));
    }

    struct StructEntry *storedFromStruct = scope_lookup_struct_by_type(metadata->scope, &storedToLt->type);
    struct StructField *storedField = struct_lookup_field_by_name(storedFromStruct, generate->operands[1].name.str, metadata->scope);

    struct Register *structBaseAddrReg = NULL;
    if (type_is_struct_object(&storedToLt->type))
    {
        structBaseAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[0], structBaseAddrReg);
    }
    else
    {
        structBaseAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[0], NULL);
    }

    if (!type_is_object(&storedField->variable->type))
    {
        struct Register *sourceReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], NULL);
        emit_instruction(generate, state, "\ts%c %s, %zd(%s)#:(\n",
                         riscv_select_width_char_for_size(type_get_size(&storedField->variable->type, metadata->scope)),
                         sourceReg->name,
                         storedField->offset,
                         structBaseAddrReg->name);
    }
    else
    {
        struct Register *sourceAddrReg = acquire_scratch_register(info);
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[2], sourceAddrReg);
        try_release_scratch_register(info, structBaseAddrReg);
        struct Register *fieldAddrReg = acquire_scratch_register(info);
        emit_instruction(generate, state, "\taddi %s, %s, %zu\n", fieldAddrReg->name, structBaseAddrReg->name, storedField->offset);
        struct Register *scratchReg = acquire_scratch_register(info);

        riscv_generate_internal_copy(generate, state, sourceAddrReg, fieldAddrReg, scratchReg, type_get_size(tac_get_type_of_operand(generate, 2), metadata->scope));
    }
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
void riscv_generate_code_for_tac(struct CodegenState *state,
                                 struct RegallocMetadata *metadata,
                                 struct MachineInfo *info,
                                 struct TACLine *generate,
                                 char *functionName,
                                 Stack *calledFunctionArguments)
{
    switch (generate->operation)
    {
    case TT_ASM:
        emit_instruction(generate, state, "%s\n", generate->operands[0].name.str);
        break;

    case TT_ASM_LOAD:
    {
        struct Register *loadedTo = find_register_by_name(info, generate->operands[0].name.str);
        if (loadedTo == NULL)
        {
            log_tree(LOG_FATAL, &generate->correspondingTree, "%s does not name a valid register", generate->operands[0].name.str);
        }

        size_t loadSize = type_get_size(tac_get_type_of_operand(generate, 1), metadata->scope);
        if (loadSize > sizeof(size_t))
        {
            log_tree(LOG_FATAL, &generate->correspondingTree, "Loaded variable has size %zu, which is larger than sizeof(size_t) (%zu)", loadSize, sizeof(size_t));
        }

        struct Register *placedOrFoundIn = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], loadedTo);

        if (register_compare(placedOrFoundIn, loadedTo))
        {
            emit_instruction(generate, state, "\tmv %s, %s\n", loadedTo->name, placedOrFoundIn->name);
        }
    }
    break;

    case TT_ASM_STORE:
    {
        struct Register *storedFrom = find_register_by_name(info, generate->operands[1].name.str);
        if (storedFrom == NULL)
        {
            log_tree(LOG_FATAL, &generate->correspondingTree, "%s does not name a valid register", generate->operands[1].name.str);
        }

        size_t storeSize = type_get_size(tac_get_type_of_operand(generate, 0), metadata->scope);
        if (storeSize > sizeof(size_t))
        {
            log_tree(LOG_FATAL, &generate->correspondingTree, "Stored variable has size %zu, which is larger than sizeof(size_t) (%zu)", storeSize, sizeof(size_t));
        }

        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], storedFrom);
    }
    break;

    case TT_ASSIGN:
    {
        struct Lifetime *writtenLt = lifetime_find(metadata->allLifetimes, generate->operands[0].name.variable->name);
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
            struct Register *opLocReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
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
        struct Register *op1Reg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
        struct Register *op2Reg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], NULL);

        // release any scratch registers we may have acquired by placing operands, as our write register may be able to use one of them
        try_release_scratch_register(info, op1Reg);
        try_release_scratch_register(info, op2Reg);

        struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));

        emit_instruction(generate, state, "\t%s %s, %s, %s\n", riscv_get_asm_op(generate->operation), destReg->name, op1Reg->name, op2Reg->name);
        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
    }
    break;

    case TT_BITWISE_NOT:
    {
        struct Register *op1Reg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
        struct Register *destReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));

        // FIXME: work with new register allocation
        emit_instruction(generate, state, "\txori %s, %s, -1\n", destReg->name, op1Reg->name);
        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], destReg);
    }
    break;

    case TT_LOAD:
    {
        struct Lifetime *writtenLt = lifetime_find(metadata->allLifetimes, generate->operands[0].name.variable->name);
        if (type_is_object(&writtenLt->type))
        {
            struct Register *sourceAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
            struct Register *destAddrReg = acquire_scratch_register(info);
            riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[0], destAddrReg);

            struct Register *intermediateReg = acquire_scratch_register(info);

            riscv_generate_internal_copy(generate, state, sourceAddrReg, destAddrReg, intermediateReg, type_get_size(&writtenLt->type, metadata->scope));
        }
        else
        {
            struct Register *baseReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
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

    case TT_STORE:
    {
        struct Type *srcType = tac_get_type_of_operand(generate, 1);
        size_t moveSize = type_get_size(srcType, metadata->scope);
        struct Register *destAddrReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[0], NULL);
        if (moveSize > sizeof(size_t))
        {
            struct Register *sourceAddrReg = acquire_scratch_register(info);
            riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], sourceAddrReg);
            struct Register *intermediateReg = acquire_scratch_register(info);

            riscv_generate_internal_copy(generate, state, sourceAddrReg, destAddrReg, intermediateReg, moveSize);
        }
        else
        {
            struct Register *sourceReg = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
            char storeWidth = riscv_select_width_char_for_dereference(metadata->scope, &generate->operands[0]);

            emit_instruction(generate, state, "\ts%c %s, 0(%s)\n",
                             storeWidth,
                             sourceReg->name,
                             destAddrReg->name);
        }
    }
    break;

    case TT_ADDROF:
    {
        struct Register *addrReg = pick_write_register(metadata, &generate->operands[0], acquire_scratch_register(info));
        riscv_place_addr_of_operand_in_reg(generate, state, metadata, info, &generate->operands[1], addrReg);
        riscv_write_variable(generate, state, metadata, info, &generate->operands[0], addrReg);
    }
    break;

    case TT_ARRAY_LOAD:
        riscv_emit_array_load(generate, state, metadata, info);
        break;

    case TT_ARRAY_LEA:
        riscv_emit_array_lea(generate, state, metadata, info);
        break;

    case TT_ARRAY_STORE:
        riscv_emit_array_store(generate, state, metadata, info);
        break;

    case TT_FIELD_LOAD:
        riscv_emit_struct_field_load(generate, state, metadata, info);
        break;

    case TT_FIELD_LEA:
        riscv_emit_struct_field_lea(generate, state, metadata, info);
        break;

    case TT_FIELD_STORE:
        riscv_emit_struct_field_store(generate, state, metadata, info);
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
        struct Register *operand1register = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[1], NULL);
        struct Register *operand2register = riscv_place_or_find_operand_in_register(generate, state, metadata, info, &generate->operands[2], NULL);
        emit_instruction(generate, state, "\t%s %s, %s, %s_%d\n",
                         tac_operation_get_name(generate->operation),
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

        Set *callerSavedArgLifetimes = riscv_caller_save_registers(state, &metadata->function->regalloc, info);

        riscv_emit_argument_stores(state, metadata, info, calledFunction, calledFunctionArguments, callerSavedArgLifetimes);
        set_free(callerSavedArgLifetimes);

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

        riscv_caller_restore_registers(state, &metadata->function->regalloc, info);
    }
    break;

    case TT_METHOD_CALL:
    {
        struct StructEntry *methodOf = scope_lookup_struct_by_type(metadata->scope, tac_get_type_of_operand(generate, 2));
        struct FunctionEntry *calledMethod = struct_lookup_method_by_string(methodOf, generate->operands[1].name.str);

        Set *callerSavedArgLifetimes = riscv_caller_save_registers(state, &metadata->function->regalloc, info);

        riscv_emit_argument_stores(state, metadata, info, calledMethod, calledFunctionArguments, callerSavedArgLifetimes);
        set_free(callerSavedArgLifetimes);

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

        riscv_caller_restore_registers(state, &metadata->function->regalloc, info);
    }
    break;

    case TT_ASSOCIATED_CALL:
    {
        struct StructEntry *associatedWith = scope_lookup_struct_by_type(metadata->scope, tac_get_type_of_operand(generate, 2));
        struct FunctionEntry *calledAssociated = struct_lookup_associated_function_by_string(associatedWith, generate->operands[1].name.str);

        Set *callerSavedArgLifetimes = riscv_caller_save_registers(state, &metadata->function->regalloc, info);

        riscv_emit_argument_stores(state, metadata, info, calledAssociated, calledFunctionArguments, callerSavedArgLifetimes);
        set_free(callerSavedArgLifetimes);

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

        riscv_caller_restore_registers(state, &metadata->function->regalloc, info);
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
    log(LOG_DEBUG, "Generate code for basic block %zu", block->labelNum);
    // we may pass null if we are generating the code to initialize global variables
    if (functionName != NULL)
    {
        fprintf(state->outFile, "%s_%zu:\n", functionName, block->labelNum);
    }

    Stack *calledFunctionArguments = stack_new(NULL);
    size_t lastLineNo = 0;
    Iterator *tacRunner = NULL;
    for (tacRunner = list_begin(block->TACList); iterator_gettable(tacRunner); iterator_next(tacRunner))
    {
        release_all_scratch_registers(info);

        struct TACLine *thisTac = iterator_get(tacRunner);

        char *printedTac = sprint_tac_line(thisTac);
        log(LOG_DEBUG, "Generate code for %s (alloc %s:%d)", printedTac, thisTac->allocFile, thisTac->allocLine);
        fprintf(state->outFile, "#%s\n", printedTac);
        free(printedTac);

        emit_loc(state, thisTac, &lastLineNo);
        riscv_generate_code_for_tac(state, metadata, info, thisTac, functionName, calledFunctionArguments);

        Iterator *regIterator = NULL;
        for (regIterator = array_begin(&info->allRegisters); iterator_gettable(regIterator); iterator_next(regIterator))
        {
            struct Register *examinedRegister = iterator_get(regIterator);
            if ((examinedRegister->containedLifetime != NULL) && !(lifetime_is_live_after_index(examinedRegister->containedLifetime, thisTac->index)))
            {
                examinedRegister->containedLifetime = NULL;
            }
        }
        iterator_free(regIterator);
    }
    iterator_free(tacRunner);
    stack_free(calledFunctionArguments);
}
