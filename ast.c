#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "util.h"

#include "mbcl/stack.h"

char *tokenNames[T_EOF + 1] = {
    "t_identifier",
    "t_constant",
    "t_char_literal",
    "t_string_literal",
    "t_extern",
    "t_sizeof",
    "t_asm",
    "t_asm_readvar",
    "t_asm_writevar",
    "t_variable_declaration",
    "t_type_name",
    "t_any",
    "t_u8",
    "t_u16",
    "t_u32",
    "t_u64",
    "t_struct",
    "t_generic",
    "t_generic_parameters",
    "t_struct_body",
    "t_impl",
    "t_self",
    "t_cap_self",
    "t_public",
    "t_method_call",
    "t_struct_initializer",
    "t_enum",
    "t_compound_statement",
    "t_fun",
    "t_return",
    "t_if",
    "t_else",
    "t_while",
    "t_for",
    "t_match",
    "t_match_arm",
    "t_match_arm_action",
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
    "t_semicolon",
    "t_colon",
    "t_underscore",
    "t_associated_call",
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

char *token_get_name(enum TOKEN type)
{
    return tokenNames[type];
}

struct Ast *ast_new(enum TOKEN type, char *value, char *curFile, u32 curLine, u32 curCol)
{
    struct Ast *wip = malloc(sizeof(struct Ast));
    wip->child = NULL;
    wip->sibling = NULL;
    wip->type = type;
    wip->value = value;
    wip->sourceLine = curLine;
    wip->sourceCol = curCol;
    wip->sourceFile = curFile;
    return wip;
}

void ast_insert_sibling(struct Ast *tree, struct Ast *newSibling)
{
    struct Ast *runner = tree;
    while (runner->sibling != NULL)
    {
        runner = runner->sibling;
    }

    runner->sibling = newSibling;
}

void ast_insert_child(struct Ast *tree, struct Ast *newChild)
{
    if (tree->child == NULL)
    {
        tree->child = newChild;
    }
    else
    {
        ast_insert_sibling(tree->child, newChild);
    }
}

struct Ast *ast_construct_add_sibling(struct Ast *tree, struct Ast *newSibling)
{
    if (tree == NULL)
    {
        return newSibling;
    }

    ast_insert_sibling(tree, newSibling);
    return tree;
}

struct Ast *ast_construct_add_child(struct Ast *tree, struct Ast *newChild)
{
    ast_insert_child(tree, newChild);
    return tree;
}

void ast_print(struct Ast *tree, size_t depth)
{
    if (tree->sibling != NULL)
    {
        ast_print(tree->sibling, depth);
    }

    for (size_t indentPrint = 0; indentPrint < depth; indentPrint++)
    {
        printf("\t");
    }

    printf("%d:%d - %s:%s\n", tree->sourceLine, tree->sourceCol, token_get_name(tree->type), tree->value);
    if (tree->child != NULL)
    {
        ast_print(tree->child, depth + 1);
    }
}

void ast_traverse_for_dump(FILE *outFile, struct Ast *parent, struct Ast *tree, size_t depth, Stack *ranks)
{
    if (ranks->size <= depth)
    {
        stack_push(ranks, stack_new(NULL));
    }
    Stack *currentRank = stack_peek(ranks);
    stack_push(currentRank, tree);

    fprintf(outFile, "%zu[label=\"%s\"]\n", (size_t)tree, strcmp(tree->value, "") ? tree->value : token_get_name(tree->type));
    if (parent != NULL)
    {
        fprintf(outFile, "%zu->%zu\n", (size_t)parent, (size_t)tree);
    }

    if (tree->sibling != NULL)
    {
        ast_traverse_for_dump(outFile, parent, tree->sibling, depth, ranks);
    }

    if (tree->child != NULL)
    {
        ast_traverse_for_dump(outFile, tree, tree->child, depth + 1, ranks);
    }
}

void ast_dump(FILE *outFile, struct Ast *tree)
{
    Stack *ranks = stack_new((void (*)(void *))stack_free);
    fprintf(outFile, "digraph ast {\n");
    fprintf(outFile, "edge[dir=forwrad]\n");
    ast_traverse_for_dump(outFile, NULL, tree, 0, ranks);

    Iterator *rankIterator = NULL;
    for (rankIterator = stack_bottom(ranks); iterator_gettable(rankIterator); iterator_next(rankIterator))
    {
        fprintf(outFile, "{rank = same; ");
        Stack *thisRank = iterator_get(rankIterator);
        Iterator *nodeIterator = NULL;
        for (nodeIterator = stack_bottom(thisRank); iterator_gettable(nodeIterator); iterator_next(nodeIterator))
        {
            struct Ast *nodeThisRank = iterator_get(nodeIterator);
            fprintf(outFile, "%zu; ", (size_t)nodeThisRank);
        }
        fprintf(outFile, "}\n");
        iterator_free(nodeIterator);
    }
    iterator_free(rankIterator);

    fprintf(outFile, "}\n");

    stack_free(ranks);
}

void ast_free(struct Ast *tree)
{
    struct Ast *runner = tree;
    while (runner != NULL)
    {
        if (runner->child != NULL)
        {
            ast_free(runner->child);
        }
        struct Ast *old = runner;
        runner = runner->sibling;
        free(old);
    }
}