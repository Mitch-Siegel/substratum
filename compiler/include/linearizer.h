#include "symtab.h"
#include "tac.h"

#ifndef _LINEARIZER_H_
#define _LINEARIZER_H_

int alignSize(int nBytes);

enum basicTypes selectVariableTypeForNumber(int num);

enum basicTypes selectVariableTypeForLiteral(char *literal);

void populateTACOperandFromVariable(struct TACOperand *o, struct VariableEntry *e);

struct SymbolTable *linearizeProgram(struct AST *program, int optimizationLevel);

#endif
