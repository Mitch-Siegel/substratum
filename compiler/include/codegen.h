#include "regalloc.h"
#include "symtab.h"


void PlaceLiteralInRegister(struct LinkedList *currentBlock, char *literalStr, char *destReg);

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
