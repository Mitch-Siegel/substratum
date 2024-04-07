#include "regalloc.h"
#include "symtab.h"

#ifndef _CODEGEN_H_
#define _CODEGEN_H_

#define STACK_ALIGN_BYTES ((size_t) 16)
#define MAX_ASM_LINE_SIZE ((size_t) 256)

extern char printedLine[MAX_ASM_LINE_SIZE];
extern char *registerNames[MACHINE_REGISTER_COUNT];

int ALIGNSIZE(unsigned int size);

struct CodegenContext
{
    size_t *instructionIndex;
    FILE *outFile;
};

void emitInstruction(struct TACLine *correspondingTACLine,
                     struct CodegenContext *context,
                     const char *format, ...);

// place a literal in the register specified by numerical index, return string of register's name for asm
char *PlaceLiteralStringInRegister(struct TACLine *correspondingTACLine,
                                   struct CodegenContext *context,
                                   char *literalStr,
                                   u8 destReg);

void verifyCodegenPrimitive(struct TACOperand *operand);

void WriteVariable(struct TACLine *correspondingTACLine,
                   struct CodegenContext *context,
                   struct Scope *scope,
                   struct LinkedList *lifetimes,
                   struct TACOperand *writtenTo,
                   u8 sourceRegIndex);

// places a variable in a register, with no guarantee that it is modifiable, returning the string of the register's name for asm
u8 placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
                                 struct CodegenContext *context,
                                 struct Scope *scope,
                                 struct LinkedList *lifetimes,
                                 struct TACOperand *operand,
                                 u8 registerIndex);

u8 pickWriteRegister(struct Scope *scope,
                      struct LinkedList *lifetimes,
                      struct TACOperand *operand,
                      u8 registerIndex);

u8 placeAddrOfLifetimeInReg(struct TACLine *correspondingTACLine,
                             struct CodegenContext *context,
                             struct Scope *scope,
                             struct LinkedList *lifetimes,
                             struct TACOperand *operand,
                             u8 registerIndex);

const char *SelectSignForLoad(u8 loadSize, struct Type *loaded);

char SelectWidthChar(struct Scope *scope, struct TACOperand *dataDest);

char SelectWidthCharForDereference(struct Scope *scope, struct TACOperand *dataDestP);

char SelectWidthCharForLifetime(struct Scope *scope, struct Lifetime *lifetime);

void EmitFrameStoreForSize(struct TACLine *correspondingTACLine,
                           struct CodegenContext *context,
                           enum riscvRegisters sourceReg,
                           u8 size,
                           ssize_t offset);

void EmitFrameLoadForSize(struct TACLine *correspondingTACLine,
                          struct CodegenContext *context,
                          enum riscvRegisters destReg,
                          u8 size,
                          ssize_t offset);

void EmitStackStoreForSize(struct TACLine *correspondingTACLine,
                           struct CodegenContext *context,
                           enum riscvRegisters sourceReg,
                           u8 size,
                           ssize_t offset);

void EmitStackLoadForSize(struct TACLine *correspondingTACLine,
                          struct CodegenContext *context,
                          enum riscvRegisters sourceReg,
                          u8 size,
                          ssize_t offset);

void EmitPushForOperand(struct TACLine *correspondingTACLine,
                        struct CodegenContext *context,
                        struct Scope *scope,
                        struct TACOperand *dataSource,
                        u8 srcRegister);

void EmitPushForSize(struct TACLine *correspondingTACLine,
                     struct CodegenContext *context,
                     u8 size,
                     u8 srcRegister);

void EmitPopForOperand(struct TACLine *correspondingTACLine,
                       struct CodegenContext *context,
                       struct Scope *scope,
                       struct TACOperand *dataDest,
                       u8 destRegister);

void EmitPopForSize(struct TACLine *correspondingTACLine,
                    struct CodegenContext *context,
                    u8 size,
                    u8 destRegister);

#endif
