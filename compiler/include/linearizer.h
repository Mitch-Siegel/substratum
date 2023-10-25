#include "symtab.h"
#include "tac.h"

#ifndef _LINEARIZER_H_
#define _LINEARIZER_H_

int alignSize(int nBytes);

enum basicTypes selectVariableTypeForNumber(int num);

enum basicTypes selectVariableTypeForLiteral(char *literal);

void populateTACOperandFromVariable(struct TACOperand *o, struct VariableEntry *e);

// copy a type, turning any array size > 0 into an increment of indirectionlevel
void copyTypeDecayArrays(struct Type *dest, struct Type *src);

// copy over the entire TACOperand, all fields are changed
void copyTACOperandDecayArrays(struct TACOperand *dest, struct TACOperand *src);

// copy over only the type and castAsType fields, decaying array sizes to simple pointer types
void copyTACOperandTypeDecayArrays(struct TACOperand *dest, struct TACOperand *src);

struct TACLine *setUpScaleMultiplication(struct AST *tree, struct Scope *scope, int *TACIndex, int *tempNum, struct Type *pointerTypeOfToScale);

struct SymbolTable *linearizeProgram(struct AST *program, int optimizationLevel);

// check the LHS of any dot operator make sure it is both a class and not indirect
// special case handling for when tree is an identifier vs a subexpression
void checkAccessedClassForDot(struct AST *tree, struct Scope *scope, struct Type *type);

// check the LHS of any arrow operator, make sure it is only a class pointer and nothing else
// special case handling for when tree is an identifier vs a subexpression
void checkAccessedClassForArrow(struct AST *tree, struct Scope *scope, struct Type *type);

#endif
