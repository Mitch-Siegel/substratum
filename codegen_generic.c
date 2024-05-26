#include "codegen_generic.h"

#include "log.h"
#include "symtab.h"
#include "tac.h"
#include "util.h"
#include <stdarg.h>

void emitInstruction(struct TACLine *correspondingTACLine,
                     struct CodegenState *context,
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

void emitLoc(struct CodegenState *context, struct TACLine *thisTAC, size_t *lastLineNo)
{
    // don't duplicate .loc's for the same line
    // riscv64-unknown-elf-gdb (or maybe the as/ld) don't enjoy going backwards/staying put in line or column loc
    if (thisTAC->correspondingTree.sourceLine > *lastLineNo)
    {
        fprintf(context->outFile, "\t.loc 1 %d\n", thisTAC->correspondingTree.sourceLine);
        *lastLineNo = thisTAC->correspondingTree.sourceLine;
    }
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

// TODO: variable number of scratch registers?
struct Register *selectScratchRegister(struct MachineInfo *context, bool allowOverwrite)
{
    for (u8 scratchIndex = 0; scratchIndex < 3; scratchIndex++)
    {
        if ((context->tempsOccupied[scratchIndex] == 0) || allowOverwrite)
        {
            context->tempsOccupied[scratchIndex] = 1;
            return context->temps[scratchIndex];
        }
    }

    InternalError("Unable to select scratch register");
}

// TODO: variable number of scratch registers?
void invalidateScratchRegister(struct MachineInfo *context, struct Register *scratchRegister)
{
    for (u8 scratchIndex = 0; scratchIndex < 3; scratchIndex++)
    {
        if (context->temps[scratchIndex] == scratchRegister)
        {
            context->tempsOccupied[scratchIndex] = 0;
            return;
        }
    }

    InternalError("invalidateScratchRegister called on non-scratch register %s", scratchRegister->name);
}

// places an operand by name into the specified register, or returns the index of the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
struct Register *placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
                                              struct CodegenState *state,
                                              struct TACOperand *operand,
                                              struct Register *optionalScratch)
{
    // TODO: reimplement with new register allocation
    return NULL;
}

struct Register *pickWriteRegister(struct CodegenMetadata *metadata,
                                   struct TACOperand *operand,
                                   struct Register *scratchReg)
{
    struct Lifetime *relevantLifetime = Set_Find(metadata->allLifetimes, operand->name.str);
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
        InternalError("Lifetime for %s has unknown writeback location!", relevantLifetime->name);
    }

    return NULL;
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
