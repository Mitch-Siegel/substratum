#include "regalloc.h"
#include "symtab.h"

// place a literal in the register specified by numerical index, return string of register's name for asm
char *PlaceLiteralInRegister(FILE *outFile, char *literalStr, int destReg);

// write a value from a register name, to a spilled lifetime on the stack
void WriteSpilledVariable(FILE *outFile,
                          struct Scope *scope,
                          struct Lifetime *writtenTo,
                          char *sourceRegStr);

// place a spilled variable's value in the register specified by numerical index, return string of register's name for asm
char *ReadSpilledVariable(FILE *outFile,
                          struct Scope *scope,
                          int destReg,
                          struct Lifetime *readFrom);

// places a variable in a register, with no guarantee that it is modifiable, returning the string of the register's name for asm
int placeOrFindOperandInRegister(FILE *outFile,
                                 struct Scope *scope,
                                 struct LinkedList *lifetimes,
                                 struct TACOperand *operand,
                                 int registerIndex);

void generateCode(struct SymbolTable *table, FILE *outFile);

void generateCodeForFunction(FILE *outFile, struct FunctionEntry *function);

const char *SelectMovWidth(struct Scope *scope, struct TACOperand *dataDest);

const char *SelectMovWidthForLifetime(struct Scope *scope, struct Lifetime *lifetime);

const char *SelectPushWidth(struct Scope *scope, struct TACOperand *dataDest);

void GenerateCodeForBasicBlock(FILE *outFile,
                               struct BasicBlock *block,
                               struct Scope *scope,
                               struct LinkedList *lifetimes,
                               char *functionName,
                               int reservedRegisters[3]);
