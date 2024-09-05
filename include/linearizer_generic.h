#ifndef LINEARIZER_GENERIC_H
#define LINEARIZER_GENERIC_H

#include "substratum_defs.h"
struct TACOperand;
struct VariableEntry;
struct EnumDesc;
struct Type;
struct Ast;
struct Scope;
struct TACLine;
struct BasicBlock;

enum BASIC_TYPES select_variable_type_for_number(size_t num);

enum BASIC_TYPES select_variable_type_for_literal(char *literal);

struct TACOperand *get_sizeof_type(struct Ast *tree,
                                   struct BasicBlock *block,
                                   struct Scope *scope,
                                   size_t *tacIndex,
                                   struct Type *getSizeof);

struct TACLine *set_up_scale_multiplication(struct Ast *tree,
                                            struct BasicBlock *block,
                                            struct Scope *scope,
                                            size_t *TACIndex,
                                            struct Type *pointerTypeOfToScale,
                                            struct Type *offsetType);

// check the LHS of any dot operator make sure it is both a struct and has an indirection level of at most `
// special case handling for when tree is an identifier vs a subexpression
void check_accessed_struct_for_dot(struct Ast *tree, struct Scope *scope, struct Type *type);

// in the case that we know we just walked an array ref, convert its direct load to an LEA (for cases such as &thing[0] and foo[1].bar)
// returns true if a conversion occurred, false if not
bool convert_array_load_to_lea(struct TACLine *loadLine, struct TACOperand *dest);

// in the case that we know we just walked a struct field load, but we know we actually want a pointer to the data
// returns true if a conversion occurred, false if not
bool convert_field_load_to_lea(struct TACLine *loadLine, struct TACOperand *dest);

#endif
