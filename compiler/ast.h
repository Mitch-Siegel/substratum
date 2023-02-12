#include <stdlib.h>

#pragma once

enum token
{
	p_type_name,
	p_primary_expression,
	p_unary_operator,
	p_unary_expression,
	p_expression,
	p_expression_list,
	p_variable_declaration,
	p_declaration_list,
	p_variable_declaration_statement,
	p_expression_statement,
	p_statement,
	p_statement_list,
	p_scope,
	p_function_definition,
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
	t_plus,
	t_minus,
	// comparison operators
	t_lThan,
	t_gThan,
	// logical operators
	t_and,
	t_or,
	t_not,
	// bitwise operators
	t_bit_not,
	t_xor,
	// ternary
	t_ternary,
	// arithmetic-assign operators
	// unary operators
	t_reference,
	t_star,
	// assignment
	t_single_equals,
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