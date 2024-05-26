#ifndef CODEGEN_GENERIC_H
#define CODEGEN_GENERIC_H
#include "regalloc_generic.h"
#include "substratum_defs.h"

#define STACK_ALIGN_BYTES ((size_t)16)
#define MAX_ASM_LINE_SIZE ((size_t)256)

extern char *registerNames[MACHINE_REGISTER_COUNT];

struct CodegenState
{
    size_t *instructionIndex;
    FILE *outFile;
};

void emitInstruction(struct TACLine *correspondingTACLine,
                     struct CodegenState *state,
                     const char *format, ...);

void emitLoc(struct CodegenState *context, struct TACLine *thisTAC, size_t *lastLineNo);

void verifyCodegenPrimitive(struct TACOperand *operand);

struct Register *acquireScratchRegister(struct MachineInfo *info);

void releaseScratchRegister(struct MachineInfo *info, struct Register *reg);

void tryReleaseScratchRegister(struct MachineInfo *info, struct Register *reg);

void releaseAllScratchRegisters(struct MachineInfo *info);

void invalidateScratchRegister(struct MachineInfo *info, struct Register *scratchRegister);

struct Register *pickWriteRegister(struct CodegenMetadata *metadata,
                                   struct TACOperand *operand,
                                   struct Register *scratchReg);

const char *SelectSignForLoad(u8 loadSize, struct Type *loaded);

char SelectWidthCharForSize(u8 size);

char SelectWidthChar(struct Scope *scope, struct TACOperand *dataDest);

char SelectWidthCharForDereference(struct Scope *scope, struct TACOperand *dataDestP);

char SelectWidthCharForLifetime(struct Scope *scope, struct Lifetime *lifetime);

#endif
