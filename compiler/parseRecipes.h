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

        // UNARY-EXPRESSION
        {{p_unary_expression, above},
         {p_null, p_null}},

        // end
        {{p_null, p_null}},
    },

    // p_unary_operator - UNARY-OPERATOR
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

    // p_unary_expression - UNARY-EXPRESSION
    {
        // '*' PRIMARY-EXPRESSION
        {{t_star, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // UNARY-OPERATOR PRIMARY-EXPRESSION
        {{p_unary_operator, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '(' ')'
        {{p_primary_expression, above},
         {t_lParen, below},
         {t_rParen, below},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '(' PRIMARY-EXPRESSION ')'
        {{p_primary_expression, above},
         {t_lParen, below},
         {p_primary_expression, below},
         {t_rParen, below},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '(' EXPRESSION ')'
        {{p_primary_expression, above},
         {t_lParen, below},
         {p_expression, below},
         {t_rParen, below},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '(' EXPRESSION-LIST ')'
        {{p_primary_expression, above},
         {t_lParen, below},
         {p_expression_list, below},
         {t_rParen, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_expression - EXPRESSION
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

    // p_expression_list - EXPRESSION-LIST
    {
        //  PRIMARY_EXPRESSION ',' PRIMARY_EXPRESSION
        {{p_primary_expression, above},
         {t_comma, cnsme},
         {p_primary_expression, besid},
         {p_null, p_null}},

        //  PRIMARY_EXPRESSION ',' EXPRESSION
        {{p_primary_expression, above},
         {t_comma, cnsme},
         {p_expression, besid},
         {p_null, p_null}},

        //  EXPRESSION ',' PRIMARY_EXPRESSION
        {{p_expression, besid},
         {t_comma, cnsme},
         {p_primary_expression, above},
         {p_null, p_null}},

        //  EXPRESSION ',' EXPRESSION
        {{p_expression, besid},
         {t_comma, cnsme},
         {p_expression, above},
         {p_null, p_null}},

        // EXPRESSION_LIST ',' PRIMARY-EXPRESSION
        {{p_expression_list, above},
         {t_comma, cnsme},
         {p_primary_expression, besid},
         {p_null, p_null}},

        // EXPRESSION_LIST ',' EXPRESSION
        {{p_expression_list, above},
         {t_comma, cnsme},
         {p_expression, besid},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_variable_declaration - VARIABLE-DECLARATION
    {
        // TYPE-NAME PRIMARY-EXPRESSION
        {{p_type_name, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_declaration_list - DECLARATION-LIST
    {
        // VARIABLE-DECLARATION VARIABLE-DECLARATION
        {{p_variable_declaration, above},
         {t_comma, cnsme},
         {p_variable_declaration, besid},
         {p_null, p_null}},

        // DECLARATION-LIST VARIABLE-DECLARATION
        {{p_declaration_list, above},
         {t_comma, cnsme},
         {p_variable_declaration, besid},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_variable_declaration_statement - VARIABLE-DECLARATION-STATEMENT
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

    // p_expression_statement - EXPRESSION-STATEMENT
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

    // p_statement - STATEMENT
    {
        {{p_variable_declaration_statement, above},
         {p_null, p_null}},

        {{p_expression_statement, above},
         {p_null, p_null}},

        {{p_primary_expression, above},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        {{t_asm, above},
         {t_rCurly, cnsme},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_statement_list - STATEMENT-LIST
    {
        {{p_statement, above},
         {p_statement, besid},
         {p_null, p_null}},

        {{p_statement_list, above},
         {p_statement, besid},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_scope - SCOPE
    {
        {{t_lCurly, above},
         {p_statement_list, below},
         {t_rCurly, below},
         {p_null, p_null}},

        {{t_lCurly, above},
         {p_statement, below},
         {t_rCurly, below},
         {p_null, p_null}},

        {{t_lCurly, above},
         {t_rCurly, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_function_definition - FUNCTION-DEFINITION
    {
        // multiple arguments
        // 'fun' PRIMARY-EXPRESSION '(' DECLARATION-LIST ')' ':' TYPE-NAME SCOPE
        {{t_fun, above},
         {p_primary_expression, below},
         {t_lParen, below},
         {p_declaration_list, below},
         {t_rParen, below},
         {t_colon, cnsme},
         {p_type_name, below},
         {p_scope, below},
         {p_null, p_null}},

        // 1 argument
        // 'fun' PRIMARY-EXPRESSION '(' VARIABLE-DECLARATION ')' ':' TYPE-NAME SCOPE
        {{t_fun, above},
         {p_primary_expression, below},
         {t_lParen, below},
         {p_variable_declaration, below},
         {t_rParen, below},
         {t_colon, cnsme},
         {p_type_name, below},
         {p_scope, below},
         {p_null, p_null}},

        // no arguments - name() becomes PRIMARY-EXPRESSION - parens are consumed automatically
        // 'fun' PRIMARY-EXPRESSION ':' TYPE-NAME SCOPE
        {{t_fun, above},
         {p_primary_expression, below},
         {t_colon, cnsme},
         {p_type_name, below},
         {p_scope, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_translation_unit - TRANSLATION-UNIT
    {
        {{p_function_definition, above},
         {p_null, p_null}},

        {{p_translation_unit, above},
         {p_translation_unit, besid},
         {p_null, p_null}},

        {{p_statement, above},
         {p_translation_unit, besid},
         {p_null, p_null}},

        {{p_translation_unit, above},
         {p_statement, besid},
         {p_null, p_null}},

        {{p_null, p_null}}}

};
