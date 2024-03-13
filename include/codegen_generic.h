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
                     struct CodegenContext *c,
                     const char *format, ...);

// place a literal in the register specified by numerical index, return string of register's name for asm
char *PlaceLiteralStringInRegister(struct TACLine *correspondingTACLine,
                                   struct CodegenContext *c,
                                   char *literalStr,
                                   int destReg);

void verifyCodegenPrimitive(struct TACOperand *operand);

void WriteVariable(struct TACLine *correspondingTACLine,
                   struct CodegenContext *c,
                   struct Scope *scope,
                   struct LinkedList *lifetimes,
                   struct TACOperand *writtenTo,
                   int sourceRegIndex);

// places a variable in a register, with no guarantee that it is modifiable, returning the string of the register's name for asm
int placeOrFindOperandInRegister(struct TACLine *correspondingTACLine,
                                 struct CodegenContext *c,
                                 struct Scope *scope,
                                 struct LinkedList *lifetimes,
                                 struct TACOperand *operand,
                                 int registerIndex);

int pickWriteRegister(struct Scope *scope,
                      struct LinkedList *lifetimes,
                      struct TACOperand *operand,
                      int registerIndex);

int placeAddrOfLifetimeInReg(struct TACLine *correspondingTACLine,
                             struct CodegenContext *c,
                             struct Scope *scope,
                             struct LinkedList *lifetimes,
                             struct TACOperand *operand,
                             int registerIndex);

const char *SelectSignForLoad(char loadSize, struct Type *loaded);

char SelectWidth(struct Scope *scope, struct TACOperand *dataDest);

char SelectWidthForDereference(struct Scope *scope, struct TACOperand *dataDestP);

char SelectWidthForLifetime(struct Scope *scope, struct Lifetime *lifetime);

void EmitPushForOperand(struct TACLine *correspondingTACLine,
                        struct CodegenContext *c,
                        struct Scope *scope,
                        struct TACOperand *dataSource,
                        int srcRegister);

void EmitPushForSize(struct TACLine *correspondingTACLine,
                     struct CodegenContext *c,
                     int size,
                     int srcRegister);

void EmitPopForOperand(struct TACLine *correspondingTACLine,
                       struct CodegenContext *c,
                       struct Scope *scope,
                       struct TACOperand *dataDest,
                       int destRegister);

void EmitPopForSize(struct TACLine *correspondingTACLine,
                    struct CodegenContext *c,
                    int size,
                    int destRegister);

#endif
