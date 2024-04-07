#include "regalloc.h"
#include "symtab.h"

#ifndef _CODEGEN_H_
#define _CODEGEN_H_

#define MACHINE_REGISTER_SIZE_BYTES 8
#define STACK_ALIGN_BYTES 16
#define MAX_ASM_LINE_SIZE 256

extern char printedLine[MAX_ASM_LINE_SIZE];
extern char *registerNames[MACHINE_REGISTER_COUNT];

int ALIGNSIZE(unsigned int size);

struct CodegenContext
{
    int *instructionIndex;
    FILE *outFile;
};

void emitInstruction(struct TACLine *correspondingTACLine,
                     struct CodegenContext *context,
                     const char *format, ...);

// place a literal in the register specified by numerical index, return string of register's name for asm
char *PlaceLiteralStringInRegister(struct TACLine *correspondingTACLine,
                                   struct CodegenContext *context,
                                   char *literalStr,
                                   int destReg);

void verifyCodegenPrimitive(struct TACOperand *operand);

void WriteVariable(struct TACLine *correspondingTACLine,
                   struct CodegenContext *context,
                   struct Scope *scope,
                   struct LinkedList *lifetimes,
                   struct TACOperand *writtenTo,
                   int sourceRegIndex);

// places a variable in a register, with no guarantee that it is modifiable, returning the string of the register's name for asm
int placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
                                 struct CodegenContext *context,
                                 struct Scope *scope,
                                 struct LinkedList *lifetimes,
                                 struct TACOperand *operand,
                                 int registerIndex);

int pickWriteRegister(struct Scope *scope,
                      struct LinkedList *lifetimes,
                      struct TACOperand *operand,
                      int registerIndex);

int placeAddrOfLifetimeInReg(struct TACLine *correspondingTACLine,
                             struct CodegenContext *context,
                             struct Scope *scope,
                             struct LinkedList *lifetimes,
                             struct TACOperand *operand,
                             int registerIndex);

const char *SelectSignForLoad(u8 loadSize, struct Type *loaded);

char SelectWidthChar(struct Scope *scope, struct TACOperand *dataDest);

char SelectWidthCharForDereference(struct Scope *scope, struct TACOperand *dataDestP);

char SelectWidthCharForLifetime(struct Scope *scope, struct Lifetime *lifetime);

void EmitFrameStoreForSize(struct TACLine *correspondingTACLine,
                           struct CodegenContext *context,
                           enum riscvRegisters sourceReg,
                           int size,
                           int offset);

void EmitFrameLoadForSize(struct TACLine *correspondingTACLine,
                          struct CodegenContext *context,
                          enum riscvRegisters destReg,
                          int size,
                          int offset);

void EmitStackStoreForSize(struct TACLine *correspondingTACLine,
                           struct CodegenContext *context,
                           enum riscvRegisters sourceReg,
                           int size,
                           int offset);

void EmitStackLoadForSize(struct TACLine *correspondingTACLine,
                          struct CodegenContext *context,
                          enum riscvRegisters sourceReg,
                          int size,
                          int offset);

void EmitPushForOperand(struct TACLine *correspondingTACLine,
                        struct CodegenContext *context,
                        struct Scope *scope,
                        struct TACOperand *dataSource,
                        int srcRegister);

void EmitPushForSize(struct TACLine *correspondingTACLine,
                     struct CodegenContext *context,
                     int size,
                     int srcRegister);

void EmitPopForOperand(struct TACLine *correspondingTACLine,
                       struct CodegenContext *context,
                       struct Scope *scope,
                       struct TACOperand *dataDest,
                       int destRegister);

void EmitPopForSize(struct TACLine *correspondingTACLine,
                    struct CodegenContext *context,
                    int size,
                    int destRegister);

#endif
