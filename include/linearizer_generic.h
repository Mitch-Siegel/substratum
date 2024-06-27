#ifndef LINEARIZER_GENERIC_H
#define LINEARIZER_GENERIC_H

#include "substratum_defs.h"
struct TACOperand;
struct VariableEntry;
struct EnumEntry;
struct Type;
struct AST;
struct Scope;
struct TACLine;

#define sprintedNumberLength 32

enum BASIC_TYPES select_variable_type_for_number(size_t num);

enum BASIC_TYPES select_variable_type_for_literal(char *literal);

void populate_tac_operand_from_variable(struct TACOperand *operandToPopulate, struct VariableEntry *populateFrom);

void populate_tac_operand_from_enum_member(struct TACOperand *operandToPopulate, struct EnumEntry *theEnum, struct AST *tree);

void populate_tac_operand_as_temp(struct TACOperand *operandToPopulate, size_t *tempNum);

// copy a type, turning any array size > 0 into an increment of indirectionlevel
void copy_type_decay_arrays(struct Type *dest, struct Type *src);

// copy over the entire TACOperand, all fields are changed
void copy_tac_operand_decay_arrays(struct TACOperand *dest, struct TACOperand *src);

// copy over only the type and castAsType fields, decaying array sizes to simple pointer types
void copy_tac_operand_type_decay_arrays(struct TACOperand *dest, struct TACOperand *src);

struct TACLine *set_up_scale_multiplication(struct AST *tree, struct Scope *scope, const size_t *TACIndex, size_t *tempNum, struct Type *pointerTypeOfToScale);

// check the LHS of any dot operator make sure it is both a struct and has an indirection level of at most `
// special case handling for when tree is an identifier vs a subexpression
void check_accessed_struct_for_dot(struct AST *tree, struct Scope *scope, struct Type *type);

// in the case that we know we just walked an array ref or member access, convert its direct load to an LEA (for cases such as &thing[0] and foo[1].bar)
void convert_load_to_lea(struct TACLine *loadLine, struct TACOperand *dest);

#endif
