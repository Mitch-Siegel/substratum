#ifndef CODEGEN_GENERIC_H
#define CODEGEN_GENERIC_H
#include "regalloc_generic.h"
#include "substratum_defs.h"

#define STACK_ALIGN_BYTES ((size_t)16)
#define MAX_ASM_LINE_SIZE ((size_t)256)

struct CodegenState
{
    size_t *instructionIndex;
    FILE *outFile;
};

void emit_instruction(struct TACLine *correspondingTACLine,
                      struct CodegenState *state,
                      const char *format, ...);

void emit_loc(struct CodegenState *context, struct TACLine *thisTAC, size_t *lastLineNo);

void verify_codegen_primitive(struct TACOperand *operand);

struct Register *acquire_scratch_register(struct MachineInfo *info);

void try_release_scratch_register(struct MachineInfo *info, struct Register *reg);

void release_all_scratch_registers(struct MachineInfo *info);

struct Register *pick_write_register(struct RegallocMetadata *metadata,
                                     struct TACOperand *operand,
                                     struct Register *scratchReg);

#endif
