#include <stdlib.h>

#pragma once


enum token
{
	t_asm,
	t_var,
	t_fun,
	t_return,
	t_if,
	t_else,
	t_while,
	t_do,
	t_name,
	t_literal,
	t_bin_add,
	t_bin_sub,
	t_bin_lThan,
	t_bin_gThan,
	t_bin_lThanE,
	t_bin_gThanE,
	t_bin_equals,
	t_bin_notEquals,
	t_bin_log_and,
	t_bin_log_or,
	t_un_log_not,
	t_reference,
	t_dereference,
	t_assign,
	t_comma,
	t_semicolon,
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

struct AST *AST_New(enum token t, char *value);

void AST_InsertSibling(struct AST *it, struct AST *newSibling);

void AST_InsertChild(struct AST *it, struct AST *newChild);

void AST_Print(struct AST *it, int depth);

void AST_PrintHorizontal(struct AST *it);

void AST_Free(struct AST *it);