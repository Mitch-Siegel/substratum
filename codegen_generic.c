#include "codegen_generic.h"

#include "log.h"
#include "symtab.h"
#include "tac.h"
#include "util.h"
#include <stdarg.h>

void emit_instruction(struct TACLine *correspondingTACLine,
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

void emit_loc(struct CodegenState *context, struct TACLine *thisTAC, size_t *lastLineNo)
{
    // don't duplicate .loc's for the same line
    // riscv64-unknown-elf-gdb (or maybe the as/ld) don't enjoy going backwards/staying put in line or column loc
    if (thisTAC->correspondingTree.sourceLine > *lastLineNo)
    {
        fprintf(context->outFile, "\t.loc 1 %d\n", thisTAC->correspondingTree.sourceLine);
        *lastLineNo = thisTAC->correspondingTree.sourceLine;
    }
}

void verify_codegen_primitive(struct TACOperand *operand)
{
    struct Type *realType = tac_operand_get_type(operand);
    if (realType->basicType == VT_STRUCT)
    {
        if (realType->pointerLevel == 0)
        {
            char *typeName = type_get_name(realType);
            InternalError("Error in verifyCodegenPrimitive: %s is not a primitive type!", typeName);
        }
    }
}

struct Register *acquire_scratch_register(struct MachineInfo *info)
{
    for (size_t scratchIndex = 0; scratchIndex < info->temps.size; scratchIndex++)
    {
        if (array_at(&info->tempsOccupied, scratchIndex) == NULL)
        {
            struct Register *tempReg = array_at(&info->temps, scratchIndex);
            array_emplace(&info->tempsOccupied, scratchIndex, tempReg);
            return tempReg;
        }
    }

    InternalError("Unable to select scratch register");
}

void release_all_scratch_registers(struct MachineInfo *info)
{
    for (size_t scratchIndex = 0; scratchIndex < info->temps.size; scratchIndex++)
    {
        array_emplace(&info->tempsOccupied, scratchIndex, NULL);
    }
}

void try_release_scratch_register(struct MachineInfo *info, struct Register *reg)
{
    for (size_t scratchIndex = 0; scratchIndex < info->temps.size; scratchIndex++)
    {
        if (array_at(&info->temps, scratchIndex) == reg)
        {
            if (array_at(&info->tempsOccupied, scratchIndex) == reg)
            {
                array_emplace(&info->tempsOccupied, scratchIndex, NULL);
            }
        }
    }
}

struct Register *pick_write_register(struct RegallocMetadata *metadata,
                                     struct TACOperand *operand,
                                     struct Register *scratchReg)
{
    struct Lifetime *relevantLifetime = lifetime_find(metadata->allLifetimes, operand->name.variable->name);
    if (relevantLifetime == NULL)
    {
        InternalError("Unable to find lifetime for variable %s!", operand->name.variable->name);
    }

    switch (relevantLifetime->wbLocation)
    {
    case WB_REGISTER:
        return relevantLifetime->writebackInfo.regLocation;

    case WB_STACK:
    case WB_GLOBAL:
        return scratchReg;

    case WB_UNKNOWN:
        InternalError("Lifetime for %s has unknown writeback location!", relevantLifetime->name);
    }

    return NULL;
}
