#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "util.h"

char *token_names[t_EOF + 1] = {
    "t_identifier",
    "t_constant",
    "t_char_literal",
    "t_string_literal",
    "t_extern",
    "t_sizeof",
    "t_asm",
    "t_variable_declaration",
    "t_type_name",
    "t_void",
    "t_u8",
    "t_u16",
    "t_u32",
    "t_u64",
    "t_class",
    "t_class_body",
    "t_compound_statement",
    "t_fun",
    "t_return",
    "t_if",
    "t_else",
    "t_while",
    "t_for",
    "t_do",
    "t_array_index",
    "t_function_call",
    "t_add",
    "t_subtract",
    "t_multiply",
    "t_divide",
    "t_modulo",
    "t_lshift",
    "t_rshift",
    "t_plus_equals",
    "t_minus_equals",
    "t_times_equals",
    "t_divide_equals",
    "t_modulo_equals",
    "t_bitwise_and_equals",
    "t_bitwise_or_equals",
    "t_bitwise_xor_equals",
    "t_lshift_equals",
    "t_rshift_equals",
    "t_less_than",
    "t_greater_than",
    "t_less_than_equals",
    "t_greater_than_equals",
    "t_equals",
    "t_not_equals",
    "t_logical_and",
    "t_logical_or",
    "t_logical_not",
    "t_bitwise_and",
    "t_bitwise_or",
    "t_bitwise_not",
    "t_bitwise_xor",
    "t_ternary",
    "t_dereference",
    "t_address_of",
    "t_assign",
    "t_cast",
    "t_comma",
    "t_dot",
    "t_arrow",
    "t_semicolon",
    "t_colon",
    "t_left_paren",
    "t_right_paren",
    "t_left_curly",
    "t_right_curly",
    "t_left_bracket",
    "t_right_bracket",
    "t_file",
    "t_line",
    "t_EOF",
};

char *getTokenName(enum token type)
{
    return token_names[type];
}

struct AST *AST_New(enum token type, char *value, char *curFile, u32 curLine, u32 curCol)
{
    struct AST *wip = malloc(sizeof(struct AST));
    wip->child = NULL;
    wip->sibling = NULL;
    wip->type = type;
    wip->value = value;
    wip->sourceLine = curLine;
    wip->sourceCol = curCol;
    wip->sourceFile = curFile;
    return wip;
}

void AST_InsertSibling(struct AST *tree, struct AST *newSibling)
{
    struct AST *runner = tree;
    while (runner->sibling != NULL)
    {
        runner = runner->sibling;
    }

    runner->sibling = newSibling;
}

void AST_InsertChild(struct AST *tree, struct AST *newChild)
{
    if (tree->child == NULL)
    {
        tree->child = newChild;
    }
    else
    {
        AST_InsertSibling(tree->child, newChild);
    }
}

struct AST *AST_ConstructAddSibling(struct AST *tree, struct AST *newSibling)
{
    if (tree == NULL)
    {
        return newSibling;
    }

    AST_InsertSibling(tree, newSibling);
    return tree;
}

struct AST *AST_ConstructAddChild(struct AST *tree, struct AST *newChild)
{
    AST_InsertChild(tree, newChild);
    return tree;
}

void AST_Print(struct AST *tree, size_t depth)
{
    if (tree->sibling != NULL)
    {
        AST_Print(tree->sibling, depth);
    }

    for (size_t indentPrint = 0; indentPrint < depth; indentPrint++)
    {
        printf("\t");
    }

    printf("%d:%d - %s:%s\n", tree->sourceLine, tree->sourceCol, getTokenName(tree->type), tree->value);
    if (tree->child != NULL)
    {
        AST_Print(tree->child, depth + 1);
    }
}

void AST_Free(struct AST *tree)
{
    struct AST *runner = tree;
    while (runner != NULL)
    {
        if (runner->child != NULL)
        {
            AST_Free(runner->child);
        }
        struct AST *old = runner;
        runner = runner->sibling;
        free(old);
    }
}