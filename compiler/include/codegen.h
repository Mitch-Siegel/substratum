#include "regalloc.h"
#include "symtab.h"

#ifndef _CODEGEN_H_
#define _CODEGEN_H_

#define MAX_ASM_LINE_SIZE 256

extern char printedLine[MAX_ASM_LINE_SIZE];
extern char *registerNames[MACHINE_REGISTER_COUNT];

int ALIGNSIZE(unsigned int size);

// place a literal in the register specified by numerical index, return string of register's name for asm
char *PlaceLiteralStringInRegister(FILE *outFile, char *literalStr, int destReg);

char *PlaceLiteralInRegister(FILE *outFile, int literal, int destReg);

void verifyCodegenPrimitive(struct TACOperand *operand);

void WriteVariable(FILE *outFile,
                   struct Scope *scope,
                   struct LinkedList *lifetimes,
                   struct TACOperand *writtenTo,
                   int sourceRegIndex);

// places a variable in a register, with no guarantee that it is modifiable, returning the string of the register's name for asm
int placeOrFindOperandInRegister(FILE *outFile,
                                 struct Scope *scope,
                                 struct LinkedList *lifetimes,
                                 struct TACOperand *operand,
                                 int registerIndex);

int pickWriteRegister(struct Scope *scope,
                      struct LinkedList *lifetimes,
                      struct TACOperand *operand,
                      int registerIndex);

int placeAddrOfLifetimeInReg(FILE *file,
                             struct Scope *scope,
                             struct LinkedList *lifetimes,
                             struct TACOperand *operand,
                             int registerIndex);

const char *SelectWidth(struct Scope *scope, struct TACOperand *dataDest);

const char *SelectWidthForDereference(struct Scope *scope, struct TACOperand *dataDestP);

const char *SelectWidthForLifetime(struct Scope *scope, struct Lifetime *lifetime);

void generateCode(struct SymbolTable *table, FILE *outFile, int regAllocOpt, int codegenOpt);

#endif
