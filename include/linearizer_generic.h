#ifndef LINEARIZER_GENERIC_H
#define LINEARIZER_GENERIC_H

#include "substratum_defs.h"
struct TACOperand;
struct VariableEntry;
struct Type;
struct AST;
struct Scope;
struct TACLine;

#define sprintedNumberLength 32

enum basicTypes selectVariableTypeForNumber(size_t num);

enum basicTypes selectVariableTypeForLiteral(char *literal);

void populateTACOperandFromVariable(struct TACOperand *operandToPopulate, struct VariableEntry *populateFrom);

void populateTACOperandAsTemp(struct TACOperand *operandToPopulate, size_t *tempNum);

// copy a type, turning any array size > 0 into an increment of indirectionlevel
void copyTypeDecayArrays(struct Type *dest, struct Type *src);

// copy over the entire TACOperand, all fields are changed
void copyTACOperandDecayArrays(struct TACOperand *dest, struct TACOperand *src);

// copy over only the type and castAsType fields, decaying array sizes to simple pointer types
void copyTACOperandTypeDecayArrays(struct TACOperand *dest, struct TACOperand *src);

struct TACLine *setUpScaleMultiplication(struct AST *tree, struct Scope *scope, const size_t *TACIndex, size_t *tempNum, struct Type *pointerTypeOfToScale);

// check the LHS of any dot operator make sure it is both a class and has an indirection level of at most `
// special case handling for when tree is an identifier vs a subexpression
void checkAccessedClassForDot(struct AST *tree, struct Scope *scope, struct Type *type);

// in the case that we know we just walked an array ref or member access, convert its direct load to an LEA (for cases such as &thing[0] and foo[1].bar)
void convertLoadToLea(struct TACLine *loadLine, struct TACOperand *dest);

#endif
