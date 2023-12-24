#include <stdlib.h>

#pragma once

enum token
{
	p_type_name,
	p_primary_expression,
	p_wip_array_access,
	p_expression_operator,
	p_wip_expression,
	p_expression,
	p_expression_tail,
	p_function_opener,
	p_function_call,
	p_expression_list,
	p_wip_array_declaration,
	p_variable_declaration,
	p_declaration_list,
	p_variable_declaration_statement,
	p_assignment_statement,
	p_return_statement,
	p_if_awating_else,
	p_if_else,
	p_if,
	p_statement,
	p_statement_list,
	p_while,
	p_scope,
	p_function_declaration,
	p_function_definition,
	p_translation_unit,
	p_null,
	// begin tokens
	t_identifier,
	t_constant,
	t_char_literal,
	t_string_literal,
	// t_sizeof,
	t_asm,
	// types
	t_void,
	t_u8,
	t_u16,
	t_u32,
	t_class,
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
	t_divide,
	t_lshift,
	t_rshift,
	// arithmetic assignment
	t_plus_equals,
	t_minus_equals,
	t_times_equals,
	t_divide_equals,
	t_bitwise_and_equals,
	t_bitwise_or_equals,
	t_bitwise_not_equals,
	t_bitwise_xor_equals,
	t_lshift_equals,
	t_rshift_equals,
	// comparison operators
	t_lThan,
	t_gThan,
	t_lThanE,
	t_gThanE,
	t_equals,
	t_nEquals,
	// logical operators
	t_logical_and,
	t_logical_or,
	t_logical_not,
	// bitwise operators
	t_bitwise_and,
	t_bitwise_or,
	t_bitwise_not,
	t_bitwise_xor,
	// ternary
	t_ternary,
	// arithmetic-assign operators
	t_star,
	// assignment
	t_single_equals,
	//
	t_comma,
	t_dot,
	t_arrow,
	t_semicolon,
	t_colon,
	t_lParen,
	t_rParen,
	t_lCurly,
	t_rCurly,
	t_lBracket,
	t_rBracket,
	t_file,
	t_line,
	t_EOF,
};

char *getTokenName(enum token t);

struct AST
{
	char *value;
	enum token type;
	struct AST *child;
	struct AST *sibling;
	int sourceLine;
	int sourceCol;
	char *sourceFile;
};

// instantiate a new AST with given type and value
// the sourceLine and sourceCol fields will be automatically populated
struct AST *AST_New(enum token t, char *value);

void AST_InsertSibling(struct AST *it, struct AST *newSibling);

void AST_InsertChild(struct AST *it, struct AST *newChild);

struct AST *AST_ConstructAddSibling(struct AST *it, struct AST *newSibling);

struct AST *AST_ConstructAddChild(struct AST *it, struct AST *newChild);

void AST_Print(struct AST *it, int depth);

void AST_PrintHorizontal(struct AST *it);

void AST_Free(struct AST *it);