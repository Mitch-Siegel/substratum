#include <stdlib.h>

#pragma once


enum token
{
	p_primary_expression,
    p_postfix_expression,
    p_argument_expression_list,
    p_unary_expression,
    p_unary_operator,
    p_cast_expression,
    p_multiplicative_expression,
    p_additive_expression,
    p_shift_expression,
    p_relational_expression,
    p_equality_expression,
    p_and_expression,
    p_exclusive_or_expression,
    p_inclusive_or_expression,
    p_logical_and_expression,
    p_logical_or_expression,
    p_conditional_expression,
    p_assignment_expression,
    p_assignment_operator,
    p_expression,
    p_constant_expression,
    p_declaration,
    p_declaration_specifiers,
    p_init_declarator_list,
    p_init_declarator,
    p_storage_class_specifier,
    p_type_specifier,
    p_struct_or_union_specifier,
    p_struct_or_union,
    p_struct_declaration_list,
    p_struct_declaration,
    p_specifier_qualifier_list,
    p_struct_declarator_list,
    p_struct_declarator,
    p_enum_specifier,
    p_enumerator_list,
    p_enumerator,
    p_type_qualifier,
    p_declarator,
    p_direct_declarator,
    p_pointer,
    p_type_qualifier_list,
    p_parameter_type_list,
    p_parameter_list,
    p_parameter_declaration,
    p_identifier_list,
    p_type_name,
    p_abstract_declarator,
    p_direct_abstract_declarator,
    p_initializer,
    p_initializer_list,
    p_statement,
    p_labeled_statement,
    p_compound_statement,
    p_declaration_list,
    p_statement_list,
    p_expression_statement,
    p_selection_statement,
    p_iteration_statement,
    p_jump_statement,
    p_translation_unit,
    p_external_declaration,
    p_function_definition,
    p_null,
	// begin tokens
	t_sizeof,
	t_asm,
	t_void,
	t_uint8,
	t_uint16,
	t_uint32,
	t_fun,
	t_return,
	t_if,
	t_else,
	t_while,
	t_for,
	t_do,
	t_identifier,
	t_constant,
	t_string_literal,
	t_un_add,
	t_un_sub,
	t_lShift,
	t_rShift,
	t_bin_lThan,
	t_bin_gThan,
	t_bin_lThanE,
	t_bin_gThanE,
	t_bin_equals,
	t_bin_notEquals,
	t_bin_log_and,
	t_bin_log_or,
	t_un_log_not,
	t_un_bit_not,
	t_un_bit_xor,
	t_un_bit_or,
	t_ternary,
	t_mul_assign,
	t_add_assign,
	t_sub_assign,
	t_lshift_assign,
	t_rshift_assign,
	t_bitand_assign,
	t_bitxor_assign,
	t_bitor_assign,
	t_un_inc,
	t_un_dec,
	t_reference,
	t_dereference,
	t_assign,
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
	t_EOF,
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