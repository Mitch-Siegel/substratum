#include "regalloc.h"
#include "symtab.h"

// place a literal in the register specified by numerical index, return string of register's name for asm
char *PlaceLiteralInRegister(FILE *outFile, char *literalStr, int destReg);

// write a value from a register name, to a spilled lifetime on the stack
void WriteSpilledVariable(FILE *outFile, struct Scope *scope, struct Lifetime *writtenTo, char* sourceRegStr);

// place a spilled variable's value in the register specified by numerical index, return string of register's name for asm
char *ReadSpilledVariable(FILE *outFile, struct Scope *scope, int destReg, struct Lifetime *readFrom);

// places a variable in a register, with no guarantee that it is modifiable, returning the string of the register's name for asm
char* placeOrFindOperandInRegister(struct LinkedList *lifetimes, struct Scope *scope, struct TACOperand operand, FILE *outFile, int registerIndex, char *touchedRegisters);

void generateCode(struct SymbolTable *table, FILE *outFile);

void generateCodeForFunction(struct FunctionEntry *function, FILE *outFile);

const char *SelectMovWidth(struct Scope *scope, struct TACOperand *dataDest);

const char *SelectMovWidthForLifetime(struct Scope *scope, struct Lifetime *lifetime);

const char *SelectPushWidth(struct Scope *scope, struct TACOperand *dataDest);

void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock,
                               struct Scope *thisScope,
                               struct LinkedList *allLifetimes,
                               char *functionName,
                               int reservedRegisters[3],
                               char *touchedRegisters,
                               FILE *outFile);
