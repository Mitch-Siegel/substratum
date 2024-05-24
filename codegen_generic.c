#include "codegen_generic.h"

#include "log.h"
#include "symtab_scope.h"
#include "tac.h"
#include "util.h"
#include <stdarg.h>

void emitInstruction(struct TACLine *correspondingTACLine,
                     struct CodegenContext *context,
                     const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(context->outFile, format, args);
    va_end(args);

    if ((correspondingTACLine != NULL) && (correspondingTACLine->asmIndex == 0))
    {
        correspondingTACLine->asmIndex = (*(context->instructionIndex))++;
    }
}

// TODO: enum for registers?
char *PlaceLiteralStringInRegister(struct TACLine *correspondingTACLine,
                                   struct CodegenContext *context,
                                   char *literalStr,
                                   u8 destReg)
{
    // TODO: reimplement with new register allocation
    // char *destRegStr = registerNames[destReg];
    // emitInstruction(correspondingTACLine, context, "\tli %s, %s # place literal\n", destRegStr, literalStr);
    // return destRegStr;
    return NULL;
}

void verifyCodegenPrimitive(struct TACOperand *operand)
{
    struct Type *realType = TACOperand_GetType(operand);
    if (realType->basicType == vt_struct)
    {
        if (realType->pointerLevel == 0)
        {
            char *typeName = Type_GetName(realType);
            InternalError("Error in verifyCodegenPrimitive: %s is not a primitive type!", typeName);
        }
    }
}

void WriteVariable(struct TACLine *correspondingTACLine,
                   struct CodegenContext *context,
                   struct Scope *scope,
                   struct LinkedList *lifetimes,
                   struct TACOperand *writtenTo,
                   u8 sourceRegIndex)
{
    // TODO: reimplement with new register allocation
}

// places an operand by name into the specified register, or returns the index of the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
u8 placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
                                struct CodegenContext *context,
                                struct Scope *scope,
                                struct LinkedList *lifetimes,
                                struct TACOperand *operand,
                                u8 registerIndex)
{
    // TODO: reimplement with new register allocation
    return 0;
}

struct Register *pickWriteRegister(struct Scope *scope,
                                   struct LinkedList *lifetimes,
                                   struct TACOperand *operand,
                                   struct Register *scratchReg)
{
    struct Lifetime *relevantLifetime = NULL; // LinkedList_Find(lifetimes, Lifetime_CompareToVariable, operand->name.str);
    if (relevantLifetime == NULL)
    {
        InternalError("Unable to find lifetime for variable %s!", operand->name.str);
    }

    switch (relevantLifetime->wbLocation)
    {
    case wb_register:
        return relevantLifetime->writebackInfo.regLocation;

    case wb_stack:
    case wb_global:
        return scratchReg;

    case wb_unknown:
    default:
        InternalError("Lifetime for %s has unknown writeback location!", relevantLifetime->name);
    }
}

u8 placeAddrOfLifetimeInReg(struct TACLine *correspondingTACLine,
                            struct CodegenContext *context,
                            struct Scope *scope,
                            struct LinkedList *lifetimes,
                            struct TACOperand *operand,
                            u8 registerIndex)
{
    // TODO: reimplement with new register allocation
    return 0;
}

char SelectWidthCharForSize(u8 size)
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
        InternalError("Error in SelectWidth: Unexpected destination variable size\n\tVariable is not pointer, and is not of size 1, 2, 4, or 8 bytes!");
    }

    return widthChar;
}

const char *SelectSignForLoad(u8 loadSize, struct Type *loaded)
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
        InternalError("Unexpected load size character seen in SelectSignForLoad!");
    }
}

char SelectWidthChar(struct Scope *scope, struct TACOperand *dataDest)
{
    // pointers and arrays (decay implicitly at this stage to pointers) are always full-width
    if (Type_GetIndirectionLevel(TACOperand_GetType(dataDest)) > 0)
    {
        return 'd';
    }

    return SelectWidthCharForSize(Type_GetSize(TACOperand_GetType(dataDest), scope));
}

char SelectWidthCharForDereference(struct Scope *scope, struct TACOperand *dataDest)
{
    struct Type *operandType = TACOperand_GetType(dataDest);
    if ((operandType->pointerLevel == 0) &&
        (operandType->basicType != vt_array))
    {
        InternalError("SelectWidthCharForDereference called on non-indirect operand %s!", dataDest->name.str);
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
    return SelectWidthCharForSize(Type_GetSize(&dereferenced, scope));
}

char SelectWidthCharForLifetime(struct Scope *scope, struct Lifetime *lifetime)
{
    char widthChar = '\0';
    if (lifetime->type.pointerLevel > 0)
    {
        widthChar = 'd';
    }
    else
    {
        widthChar = SelectWidthCharForSize(Type_GetSize(&lifetime->type, scope));
    }

    return widthChar;
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the frame pointer
void EmitFrameStoreForSize(struct TACLine *correspondingTACLine,
                           struct CodegenContext *context,
                           struct Register *sourceReg,
                           u8 size,
                           ssize_t offset)
{
    // TODO: reimplement with new register allocation
    emitInstruction(correspondingTACLine, context, "\ts%c %s, %d(fp)\n", SelectWidthCharForSize(size), sourceReg->name, offset);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the frame pointer
void EmitFrameLoadForSize(struct TACLine *correspondingTACLine,
                          struct CodegenContext *context,
                          u8 destReg,
                          u8 size,
                          ssize_t offset)
{
    // TODO: reimplement with new register allocation
    // emitInstruction(correspondingTACLine, context, "\tl%c %s, %d(fp)\n", SelectWidthCharForSize(size), registerNames[destReg], offset);
}

// emit an instruction to store store 'size' bytes from 'sourceReg' at 'offset' bytes from the stack pointer
void EmitStackStoreForSize(struct TACLine *correspondingTACLine,
                           struct CodegenContext *context,
                           u8 sourceReg,
                           u8 size,
                           ssize_t offset)
{
    // TODO: reimplement with new register allocation
    // emitInstruction(correspondingTACLine, context, "\ts%c %s, %d(sp)\n", SelectWidthCharForSize(size), registerNames[sourceReg], offset);
}

// emit an instruction to load store 'size' bytes to 'destReg' from 'offset' bytes from the stack pointer
void EmitStackLoadForSize(struct TACLine *correspondingTACLine,
                          struct CodegenContext *context,
                          u8 sourceReg,
                          u8 size,
                          ssize_t offset)
{
    // TODO: reimplement with new register allocation
    // emitInstruction(correspondingTACLine, context, "\tl%c %s, %d(sp)\n", SelectWidthCharForSize(size), registerNames[sourceReg], offset);
}

void EmitPushForOperand(struct TACLine *correspondingTACLine,
                        struct CodegenContext *context,
                        struct Scope *scope,
                        struct TACOperand *dataSource,
                        u8 srcRegister)
{
    size_t size = Type_GetSize(TACOperand_GetType(dataSource), scope);
    switch (size)
    {
    case sizeof(u8):
    case sizeof(u16):
    case sizeof(u32):
    case sizeof(u64):
        EmitPushForSize(correspondingTACLine, context, size, srcRegister);
        break;

    default:
    {
        char *typeName = Type_GetName(TACOperand_GetType(dataSource));
        InternalError("Unsupported size %zu seen in EmitPushForOperand (for type %s)", size, typeName);
    }
    }
}

void EmitPushForSize(struct TACLine *correspondingTACLine,
                     struct CodegenContext *context,
                     u8 size,
                     u8 srcRegister)
{
    // TODO: reimplement with new register allocation
    // emitInstruction(correspondingTACLine, context, "\taddi sp, sp, -%d\n", size);
    // emitInstruction(correspondingTACLine, context, "\ts%c %s, 0(sp)\n",
    //                 SelectWidthCharForSize(size),
    //                 registerNames[srcRegister]);
}

void EmitPopForOperand(struct TACLine *correspondingTACLine,
                       struct CodegenContext *context,
                       struct Scope *scope,
                       struct TACOperand *dataDest,
                       u8 destRegister)

{
    // TODO: reimplement with new register allocation
    // size_t size = Type_GetSize(TACOperand_GetType(dataDest), scope);
    // switch (size)
    // {
    // case sizeof(u8):
    // case sizeof(u16):
    // case sizeof(u32):
    // case sizeof(u64):
    //     EmitPopForSize(correspondingTACLine, context, size, destRegister);

    //     break;

    // default:
    // {
    //     char *typeName = Type_GetName(TACOperand_GetType(dataDest));
    //     InternalError("Unsupported size %zu seen in EmitPopForOperand (for type %s)", size, typeName);
    // }
    // }
}

void EmitPopForSize(struct TACLine *correspondingTACLine,
                    struct CodegenContext *context,
                    u8 size,
                    u8 destRegister)
{
    // TODO: reimplement with new register allocation
    // emitInstruction(correspondingTACLine, context, "\tl%c%s %s, 0(sp)\n",
    //                 SelectWidthCharForSize(size),
    //                 (size == MACHINE_REGISTER_SIZE_BYTES) ? "" : "u", // always generate an unsigned load (except for when loading 64 bit values, for which there is no unsigned load)
    //                 registerNames[destRegister]);
    // emitInstruction(correspondingTACLine, context, "\taddi sp, sp, %d\n", size);
}
