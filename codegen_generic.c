#include "codegen_generic.h"

#include "log.h"
#include "symtab.h"
#include "tac.h"
#include "util.h"
#include <stdarg.h>

void emitInstruction(struct TACLine *correspondingTACLine,
                     struct CodegenState *state,
                     const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(state->outFile, format, args);
    va_end(args);

    if ((correspondingTACLine != NULL) && (correspondingTACLine->asmIndex == 0))
    {
        correspondingTACLine->asmIndex = (*(state->instructionIndex))++;
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

struct Register *acquireScratchRegister(struct MachineInfo *info)
{
    for (u8 scratchIndex = 0; scratchIndex < info->n_temps; scratchIndex++)
    {
        if (info->tempsOccupied[scratchIndex] == 0)
        {
            info->tempsOccupied[scratchIndex] = 1;
            return info->temps[scratchIndex];
        }
    }

    InternalError("Unable to select scratch register");
}

void releaseScratchRegister(struct MachineInfo *info, struct Register *reg)
{
    for (u8 scratchIndex = 0; scratchIndex < info->n_temps; scratchIndex++)
    {
        if (info->temps[scratchIndex] == reg)
        {
            if (info->tempsOccupied[scratchIndex] != 0)
            {
                info->tempsOccupied[scratchIndex] = 0;
            }
            else
            {
                InternalError("Attempt to release non-held scratch register %s", reg->name);
            }
        }
    }

    InternalError("Attempt to release non-scratch register %s", reg->name);
}

void releaseAllScratchRegisters(struct MachineInfo *info)
{
    for (u8 scratchIndex = 0; scratchIndex < info->n_temps; scratchIndex++)
    {
        info->tempsOccupied[scratchIndex] = 0;
    }
}

void tryReleaseScratchRegister(struct MachineInfo *info, struct Register *reg)
{
    for (u8 scratchIndex = 0; scratchIndex < info->n_temps; scratchIndex++)
    {
        if (info->temps[scratchIndex] == reg)
        {
            if (info->tempsOccupied[scratchIndex] != 0)
            {
                info->tempsOccupied[scratchIndex] = 0;
            }
        }
    }
}

// TODO: variable number of scratch registers?
void invalidateScratchRegister(struct MachineInfo *info, struct Register *scratchRegister)
{
    for (u8 scratchIndex = 0; scratchIndex < 3; scratchIndex++)
    {
        if (info->temps[scratchIndex] == scratchRegister)
        {
            info->tempsOccupied[scratchIndex] = 0;
            return;
        }
    }

    InternalError("invalidateScratchRegister called on non-scratch register %s", scratchRegister->name);
}

struct Register *pickWriteRegister(struct RegallocMetadata *metadata,
                                   struct TACOperand *operand,
                                   struct Register *scratchReg)
{
    struct Lifetime dummyLt = {0};
    dummyLt.name = operand->name.str;
    struct Lifetime *relevantLifetime = Set_Find(metadata->allLifetimes, &dummyLt);
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
