#include <stdlib.h>

#pragma once


enum token
{
	p_parameter_decl,
	p_primary_expression,
	p_binary_expression,
	p_expression,
	p_type_specifier,
	p_declarator,
	p_variable_declaration,
	p_function_declaration,
	p_parameter_decl_list,
	p_scope,
	p_null,
	// begin tokens
	t_identifier,
	t_constant,
	t_string_literal,
	// t_sizeof,
	t_asm,
	// types
	t_void,
	t_uint8,
	t_uint16,
	t_uint32,
	// function
	t_fun,
	t_return,
	// control flow
	t_if,
	t_else,
	t_while,
	t_for,
	t_do,
	
	// arithmetic operators
	// basic arithmetic
	t_bin_add,
	t_bin_sub,
	t_lShift,
	t_rShift,
	// comparison operators
	t_bin_lThan,
	t_bin_gThan,
	t_bin_lThanE,
	t_bin_gThanE,
	t_bin_equals,
	t_bin_notEquals,
	// logical operators
	t_bin_log_and,
	t_bin_log_or,
	t_un_log_not,
	// bitwise operators
	t_un_bit_not,
	t_bin_bit_xor,
	t_bin_bit_or,
	// ternary
	t_ternary,
	// arithmetic-assign operators
	t_mul_assign,
	t_add_assign,
	t_sub_assign,
	t_lshift_assign,
	t_rshift_assign,
	t_bitand_assign,
	t_bitxor_assign,
	t_bitor_assign,
	// unary operators
	t_un_inc,
	t_un_dec,
	t_reference,
	t_star,
	// assignment
	t_assign,
	// 
	t_comma,
	t_dot,
	t_pointer_op,
	t_semicolon,
	t_colon,
	t_lParen,
	t_rParen,
	t_lCurly,
	t_rCurly,
	t_lBracket,
	t_rBracket,
	t_array,
	t_call,
	t_scope,
	t_EOF
};

struct AST
{
	char *value;
	enum token type;
	struct AST *child;
	struct AST *sibling;
	int sourceLine;
	int sourceCol;
};

// instantiate a new AST with given type and value
// the sourceLine and sourceCol fields will be automatically populated
struct AST *AST_New(enum token t, char *value);

void AST_InsertSibling(struct AST *it, struct AST *newSibling);

void AST_InsertChild(struct AST *it, struct AST *newChild);

void AST_Print(struct AST *it, int depth);

void AST_PrintHorizontal(struct AST *it);

void AST_Free(struct AST *it);