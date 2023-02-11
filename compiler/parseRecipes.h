#include "ast.h"

enum RecipeInstructions
{
    above, // insert as of current node, current node becomes the inserted one
    below, // insert as child of current node, current node stays the same
    besid, // insert as sibling of current node, current node stays the same
    cnsme  // consume rather than add, and free the associated AST
};

// let him cook!
enum token parseRecipes[p_null][8][9][2] = {
    // p_type_name
    {
        {{t_void, above},
         {p_null, p_null}},

        // 'uint8'
        {{t_uint8, above},
         {p_null, p_null}},

        // 'uint16'
        {{t_uint16, above},
         {p_null, p_null}},

        // 'uint32'
        {{t_uint32, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_primary_expression - PRIMARY-EXPRESSION
    {
        // identifier
        {{t_identifier, above},
         {p_null, p_null}},

        // constant
        {{t_constant, above},
         {p_null, p_null}},

        // '(' expression ')'
        {{t_lParen, cnsme},
         {p_expression, above},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // end
        {{p_null, p_null}},
    },

    // p_unary_operator
    {
        // '++'
        {{t_plus, above},
         {t_plus, cnsme},
         {p_null, p_null}},

        // '--'
        {{t_minus, above},
         {t_minus, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_unary_expression
    {
        // '*' PRIMARY-EXPRESSION
        {{t_star, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // UNARY-OPERATOR PRIMARY-EXPRESSION
        {{p_unary_operator, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_expression
    {
        // PRIMARY-EXPRESSION UNARY-OPERATOR PRIMARY-EXPRESSION
        {{p_primary_expression, above},
         {p_unary_operator, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '+' PRIMARY-EXPRESSION
        {{p_primary_expression, above},
         {t_plus, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // EXPRESSION '+' PRIMARY-EXPRESSION
        {{p_expression, above},
         {t_plus, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '-' PRIMARY-EXPRESSION
        {{p_primary_expression, above},
         {t_minus, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // EXPRESSION '-' PRIMARY-EXPRESSION
        {{p_expression, above},
         {t_minus, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_variable_declaration
    {
        // TYPE-NAME PRIMARY-EXPRESSION
        {{p_type_name, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_declaration_list
    {
        // VARIABLE-DECLARATION VARIABLE-DECLARATION
        {{p_variable_declaration, above},
         {p_variable_declaration, besid},
         {p_null, p_null}},

        // DECLARATION-LIST VARIABLE-DECLARATION
        {{p_declaration_list, above},
         {p_variable_declaration, besid},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_variable_declaration_statement
    {
        // VARIABLE-DECLARATION ';'
        {{p_variable_declaration, above},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        // VARIABLE-DECLARATION = 'PRIMARY-EXPRESSION ';'
        {{p_variable_declaration, above},
         {t_single_equals, above},
         {p_primary_expression, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_expression_statement
    // match expression statement down here with lower precedence to allow semicolons to stick on to other things first
    {
        // PRIMARY-EXPRESSION '=' PRIMARY-EXPRESSION ';'
        {{p_primary_expression, above},
         {t_single_equals, above},
         {p_primary_expression, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '=' EXPRESSION ';'
        {{p_primary_expression, above},
         {t_single_equals, above},
         {p_expression, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        // UNARY-EXPRESSION '=' PRIMARY-EXPRESSION ';'
        {{p_unary_expression, above},
         {t_single_equals, above},
         {p_primary_expression, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        // UNARY-EXPRESSION '=' EXPRESSION ';'
        {{p_unary_expression, above},
         {t_single_equals, above},
         {p_expression, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        // ';'
        // {{t_semicolon, above},
        //  {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_statement
    {
        {{p_variable_declaration_statement, above},
         {p_null, p_null}},

        {{p_expression_statement, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_statement_list
    {
        {{p_statement, above},
         {p_statement, besid},
         {p_null, p_null}},

        {{p_statement_list, above},
         {p_statement, besid},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_scope
    {
        {{t_lCurly, cnsme},
         {p_statement_list, above},
         {t_rCurly, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_function_definition
    {
        // multiple arguments
        // 'fun' PRIMARY-EXPRESSION '(' DECLARATION-LIST ')' ':' TYPE-NAME SCOPE
        {{t_fun, above},
         {p_primary_expression, below},
         {t_lParen, cnsme},
         {p_declaration_list, below},
         {t_rParen, cnsme},
         {t_colon, cnsme},
         {p_type_name, below},
         {p_scope, below},
         {p_null, p_null}},

        // 1 argument
        // 'fun' PRIMARY-EXPRESSION '(' VARIABLE-DECLARATION ')' ':' TYPE-NAME SCOPE
        {{t_fun, above},
         {p_primary_expression, below},
         {t_lParen, cnsme},
         {p_variable_declaration, below},
         {t_rParen, cnsme},
         {t_colon, cnsme},
         {p_type_name, below},
         {p_scope, below},
         {p_null, p_null}},

        // no arguments
        // 'fun' PRIMARY-EXPRESSION '(' ')' ':' TYPE-NAME SCOPE
        {{t_fun, above},
         {p_primary_expression, below},
         {t_lParen, cnsme},
         {t_rParen, cnsme},
         {t_colon, cnsme},
         {p_type_name, below},
         {p_scope, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    }

};
