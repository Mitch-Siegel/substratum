#include <stdlib.h>

#pragma once

enum token
{
	t_identifier,
	t_constant,
	t_char_literal,
	t_string_literal,
	// t_sizeof,
	t_asm,
	// types
	t_variable_declaration,
	t_type_name,
	t_void,
	t_u8,
	t_u16,
	t_u32,
	t_class,
	//
	t_class_body,
	t_compound_statement,
	//
	// function
	t_fun,
	t_return,
	// control flow
	t_if,
	t_else,
	t_while,
	t_for,
	t_do,
	//
	t_array_index,
	t_function_call,
	// arithmetic operators
	// basic arithmetic
	t_add,
	t_subtract,
	t_multiply,
	t_divide,
	t_modulo,
	t_lshift,
	t_rshift,
	// arithmetic assignment
	t_plus_equals,
	t_minus_equals,
	t_times_equals,
	t_divide_equals,
	t_modulo_equals,
	t_bitwise_and_equals,
	t_bitwise_or_equals,
	t_bitwise_xor_equals,
	t_lshift_equals,
	t_rshift_equals,
	// comparison operators
	t_less_than,
	t_greater_than,
	t_less_than_equals,
	t_greater_than_equals,
	t_equals,
	t_not_equals,
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
	// memory operators
	t_dereference,
	t_address_of,
	// assignment
	t_assign,
	//
	t_cast,
	t_comma,
	t_dot,
	t_arrow,
	t_semicolon,
	t_colon,
	t_left_paren,
	t_right_paren,
	t_left_curly,
	t_right_curly,
	t_left_bracket,
	t_right_bracket,
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
struct AST *AST_New(enum token t, char *value, char *curFile, int curLine, int curCol);

void AST_InsertSibling(struct AST *it, struct AST *newSibling);

void AST_InsertChild(struct AST *it, struct AST *newChild);

struct AST *AST_ConstructAddSibling(struct AST *it, struct AST *newSibling);

struct AST *AST_ConstructAddChild(struct AST *it, struct AST *newChild);

void AST_Print(struct AST *it, int depth);

void AST_PrintHorizontal(struct AST *it);

void AST_Free(struct AST *it);