#include "substratum_defs.h"

struct CodegenState;
struct MachineInfo;
struct Register;
struct TACLine;
struct TACOperand;
struct RegallocMetadata;
struct MachineInfo;
struct BasicBlock;
struct Scope;

void riscv_EmitFrameStoreForSize(struct TACLine *correspondingTACLine,
                                 struct CodegenState *state,
                                 struct MachineInfo *machineContext,
                                 struct Register *sourceReg,
                                 u8 size,
                                 ssize_t offset);

void riscv_EmitFrameLoadForSize(struct TACLine *correspondingTACLine,
                                struct CodegenState *state,
                                struct MachineInfo *machineContext,
                                struct Register *destReg,
                                u8 size,
                                ssize_t offset);

void riscv_EmitStackStoreForSize(struct TACLine *correspondingTACLine,
                                 struct CodegenState *state,
                                 struct MachineInfo *machineContext,
                                 struct Register *sourceReg,
                                 u8 size,
                                 ssize_t offset);

void riscv_EmitStackLoadForSize(struct TACLine *correspondingTACLine,
                                struct CodegenState *state,
                                struct MachineInfo *machineContext,
                                struct Register *destReg,
                                u8 size,
                                ssize_t offset);

void riscv_EmitPushForOperand(struct TACLine *correspondingTACLine,
                              struct CodegenState *state,
                              struct Scope *scope,
                              struct TACOperand *dataSource,
                              struct Register *srcRegister);

void riscv_EmitPushForSize(struct TACLine *correspondingTACLine,
                           struct CodegenState *state,
                           u8 size,
                           struct Register *srcRegister);

void riscv_EmitPopForOperand(struct TACLine *correspondingTACLine,
                             struct CodegenState *state,
                             struct Scope *scope,
                             struct TACOperand *dataDest,
                             struct Register *destRegister);

void riscv_EmitPopForSize(struct TACLine *correspondingTACLine,
                          struct CodegenState *state,
                          u8 size,
                          struct Register *destRegister);

void riscv_emitPrologue(struct CodegenState *context, struct RegallocMetadata *metadata, struct MachineInfo *info);

void riscv_emitEpilogue(struct CodegenState *context, struct RegallocMetadata *metadata, struct MachineInfo *info, char *functionName);

struct Register *riscv_placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
                                                    struct CodegenState *state,
                                                    struct RegallocMetadata *metadata,
                                                    struct MachineInfo *info,
                                                    struct TACOperand *operand,
                                                    struct Register *optionalScratch);

void riscv_WriteVariable(struct TACLine *correspondingTACLine,
                         struct CodegenState *state,
                         struct RegallocMetadata *metadata,
                         struct MachineInfo *info,
                         struct TACOperand *writtenTo,
                         struct Register *dataSource);

// place a literal in the register specified by numerical index, return string of register's name for asm
void riscv_PlaceLiteralStringInRegister(struct TACLine *correspondingTACLine,
                                        struct CodegenState *state,
                                        char *literalStr,
                                        struct Register *destReg);

// places a variable in a register, with no guarantee that it is modifiable, returning the string of the register's name for asm
struct Register *placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
                                              struct CodegenState *state,
                                              struct TACOperand *operand,
                                              struct Register *optionalScratch);

void riscv_placeAddrOfOperandInReg(struct TACLine *correspondingTACLine,
                                   struct CodegenState *state,
                                   struct RegallocMetadata *metadata,
                                   struct MachineInfo *info,
                                   struct TACOperand *operand,
                                   struct Register *destReg);

void riscv_GenerateCodeForBasicBlock(struct CodegenState *state,
                                     struct RegallocMetadata *metadata,
                                     struct MachineInfo *info,
                                     struct BasicBlock *block,
                                     char *functionName);
