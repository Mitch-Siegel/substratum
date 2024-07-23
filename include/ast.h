#include <stdlib.h>

#pragma once
#include "substratum_defs.h"
#include <stdio.h>

enum TOKEN
{
    T_IDENTIFIER,
    T_CONSTANT,
    T_CHAR_LITERAL,
    T_STRING_LITERAL,
    T_EXTERN,
    T_SIZEOF,
    T_ASM,
    T_ASM_READVAR,
    T_ASM_WRITEVAR,
    // types
    T_VARIABLE_DECLARATION,
    T_TYPE_NAME,
    T_ANY,
    T_U8,
    T_U16,
    T_U32,
    T_U64,
    T_STRUCT,
    // struct
    T_STRUCT_BODY,
    T_IMPL,
    T_SELF,
    T_CAP_SELF,
    T_PUBLIC,
    T_METHOD_CALL,
    T_STRUCT_INITIALIZER,
    T_ENUM,
    //
    T_COMPOUND_STATEMENT,
    //
    // function
    T_FUN,
    T_RETURN,
    // control flow
    T_IF,
    T_ELSE,
    T_WHILE,
    T_FOR,
    T_MATCH,
    T_MATCH_ARM,
    T_MATCH_ARM_ACTION,
    T_DO,
    //
    T_ARRAY_INDEX,
    T_FUNCTION_CALL,
    // arithmetic operators
    // basic arithmetic
    T_ADD,
    T_SUBTRACT,
    T_MULTIPLY,
    T_DIVIDE,
    T_MODULO,
    T_LSHIFT,
    T_RSHIFT,
    // arithmetic assignment
    T_PLUS_EQUALS,
    T_MINUS_EQUALS,
    T_TIMES_EQUALS,
    T_DIVIDE_EQUALS,
    T_MODULO_EQUALS,
    T_BITWISE_AND_EQUALS,
    T_BITWISE_OR_EQUALS,
    T_BITWISE_XOR_EQUALS,
    T_LSHIFT_EQUALS,
    T_RSHIFT_EQUALS,
    // comparison operators
    T_LESS_THAN,
    T_GREATER_THAN,
    T_LESS_THAN_EQUALS,
    T_GREATER_THAN_EQUALS,
    T_EQUALS,
    T_NOT_EQUALS,
    // logical operators
    T_LOGICAL_AND,
    T_LOGICAL_OR,
    T_LOGICAL_NOT,
    // bitwise operators
    T_BITWISE_AND,
    T_BITWISE_OR,
    T_BITWISE_NOT,
    T_BITWISE_XOR,
    // ternary
    T_TERNARY,
    // memory operators
    T_DEREFERENCE,
    T_ADDRESS_OF,
    // assignment
    T_ASSIGN,
    //
    T_CAST,
    T_COMMA,
    T_DOT,
    T_SEMICOLON,
    T_COLON,
    T_UNDERSCORE,
    T_ASSOCIATED_CALL,
    T_LEFT_PAREN,
    T_RIGHT_PAREN,
    T_LEFT_CURLY,
    T_RIGHT_CURLY,
    T_LEFT_BRACKET,
    T_RIGHT_BRACKET,
    T_FILE,
    T_LINE,
    T_EOF,
};

char *token_get_name(enum TOKEN type);

struct Ast
{
    char *value;
    enum TOKEN type;
    struct Ast *child;
    struct Ast *sibling;
    u32 sourceLine;
    u32 sourceCol;
    char *sourceFile;
};

// instantiate a new AST with given type and value
// the sourceLine and sourceCol fields will be automatically populated
struct Ast *ast_new(enum TOKEN type, char *value, char *curFile, u32 curLine, u32 curCol);

void ast_insert_sibling(struct Ast *tree, struct Ast *newSibling);

void ast_insert_child(struct Ast *tree, struct Ast *newChild);

struct Ast *ast_construct_add_sibling(struct Ast *tree, struct Ast *newSibling);

struct Ast *ast_construct_add_child(struct Ast *tree, struct Ast *newChild);

void ast_print(struct Ast *tree, size_t depth);

void ast_dump(FILE *outFile, struct Ast *tree);

void ast_free(struct Ast *tree);