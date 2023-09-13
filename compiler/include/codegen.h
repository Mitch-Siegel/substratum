#include "regalloc.h"
#include "symtab.h"

#ifndef _CODEGEN_H_
#define _CODEGEN_H_

#define MAX_ASM_LINE_SIZE 256

extern char printedLine[MAX_ASM_LINE_SIZE];
extern char *registerNames[MACHINE_REGISTER_COUNT];

int ALIGNSIZE(unsigned int size);

// place a literal in the register specified by numerical index, return string of register's name for asm
char *PlaceLiteralInRegister(FILE *outFile, char *literalStr, int destReg);

void verifyCodegenPrimitive(struct TACOperand *operand);

void WriteVariable(FILE *outFile,
                   struct LinkedList *lifetimes,
                   struct Scope *scope,
                   struct TACOperand *writtenTo,
                   int sourceRegIndex);

// places a variable in a register, with no guarantee that it is modifiable, returning the string of the register's name for asm
int placeOrFindOperandInRegister(FILE *outFile,
                                 struct Scope *scope,
                                 struct LinkedList *lifetimes,
                                 struct TACOperand *operand,
                                 int registerIndex);

int pickWriteRegister(struct LinkedList *lifetimes,
                      struct Scope *scope,
                      struct TACOperand *operand,
                      int registerIndex);

int placeAddrOfLifetimeInReg(FILE *file,
                             struct LinkedList *lifetimes,
                             struct Scope *scope,
                             struct TACOperand *operand,
                             int registerIndex);

const char *SelectMovWidth(struct Scope *scope, struct TACOperand *dataDest);

const char *SelectMovWidthForDereference(struct Scope *scope, struct TACOperand *dataDestP);

const char *SelectMovWidthForLifetime(struct Scope *scope, struct Lifetime *lifetime);

const char *SelectPushWidth(struct Scope *scope, struct TACOperand *dataDest);

void generateCode(struct SymbolTable *table, FILE *outFile, int regAllocOpt, int codegenOpt);

#endif
