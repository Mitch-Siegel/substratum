#include "symtab.h"
#include "tac.h"

#ifndef _LINEARIZER_H_
#define _LINEARIZER_H_

int alignSize(int nBytes);

enum basicTypes selectVariableTypeForNumber(int num);

enum basicTypes selectVariableTypeForLiteral(char *literal);

void populateTACOperandFromVariable(struct TACOperand *o, struct VariableEntry *e);

// copy over the entire TACOperand, all fields are changed
void copyTACOperandDecayArrays(struct TACOperand *dest, struct TACOperand *src);

// copy over only the type and castAsType fields, decaying array sizes to simple pointer types
void copyTACOperandTypeDecayArrays(struct TACOperand *dest, struct TACOperand *src);

struct TACLine *setUpScaleMultiplication(struct AST *tree, struct Scope *scope, int *TACIndex, int *tempNum, struct Type *pointerTypeOfToScale);

struct SymbolTable *linearizeProgram(struct AST *program, int optimizationLevel);

#endif
