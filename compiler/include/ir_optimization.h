#include "tac.h"
#include "symtab.h"

void optimizeIRForBasicBlock(struct Scope *scope, struct BasicBlock *block);

void optimizeIR(struct SymbolTable *table);