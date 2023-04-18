#include "regalloc.h"
#include "symtab.h"

void PlaceLiteralInRegister(struct LinkedList *currentBlock, char *literalStr, int destReg);

void WriteSpilledVariable(struct LinkedList *currentBlock, struct Lifetime *writtenTo, int sourceReg);

void ReadSpilledVariable(struct LinkedList *currentBlock, int destReg, struct Lifetime *readFrom);

// places a variable in a register, with no guarantee that it is modifiable
int placeOrFindOperandInRegister(struct LinkedList *lifetimes, struct TACOperand operand, struct LinkedList *currentBlock, int registerIndex, char *touchedRegisters);

struct Stack *generateCode(struct SymbolTable *table, FILE *outFile);

struct LinkedList *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile);

const char *SelectMovWidthForPrimitive(enum variableTypes type);

const char *SelectMovWidth(struct TACOperand *dataDest);

const char *SelectPushWidthForPrimitive(enum variableTypes type);

const char *SelectPushWidth(struct TACOperand *dataDest);

void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock,
                               struct Scope *thisScope,
                               struct LinkedList *allLifetimes,
                               struct LinkedList *asmBlock,
                               char *functionName,
                               int reservedRegisters[2],
                               char *touchedRegisters);
