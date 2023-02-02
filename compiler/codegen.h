#include "regalloc.h"
#include "symtab.h"


void PlaceLiteralInRegister(struct LinkedList *currentBlock, char *literalStr, char *destReg);

struct Stack *generateCode(struct SymbolTable *table, FILE *outFile);

struct Stack *generateCodeForScope(struct Scope *scope, FILE *outFile);

struct LinkedList *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile);

const char *SelectMovWidthForPrimitive(enum variableTypes type);

const char *SelectMovWidth(struct TACOperand *dataDest, struct Scope *currentScope);

void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock,
                               struct Scope *thisScope,
                               struct LinkedList *allLifetimes,
                               struct LinkedList *asmBlock,
                               char *functionName,
                               int reservedRegisters[2],
                               char *touchedRegisters);
