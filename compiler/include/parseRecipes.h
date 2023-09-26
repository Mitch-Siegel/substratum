#include "ast.h"

enum RecipeInstructions
{
    above, // insert as of current node, current node becomes the inserted one
    below, // insert as child of current node, current node stays the same
    besid, // insert as sibling of current node, current node stays the same
    cnsme  // consume rather than add, and free the associated AST
};

// let him cook!
enum token parseRecipes[p_null][14][9][2] = {
    // p_type_name - TYPE-NAME
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

        {{t_class, above},
         {t_identifier, below},
         {p_null, p_null}},

        {{p_type_name, above},
         {t_star, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_primary_expression - PRIMARY-EXPRESSION
    {
        // identifier
        {{t_identifier, above},
         {p_null, p_null}},

        {{p_primary_expression, above},
         {t_dot, above},
         {t_identifier, below},
         {p_null, p_null}},

        {{p_primary_expression, above},
         {t_arrow, above},
         {t_identifier, below},
         {p_null, p_null}},

        // constant
        {{t_constant, above},
         {p_null, p_null}},

        // '(' EXPRESSION ')'
        {{t_lParen, cnsme},
         {p_expression, above},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // '(' PRIMARY-EXPRESSION ')'
        {{t_lParen, cnsme},
         {p_primary_expression, above},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // UNARY-EXPRESSION
        {{p_unary_expression, above},
         {p_null, p_null}},

        {{t_char_literal, above},
         {p_null, p_null}},

        {{t_string_literal, above},
         {p_null, p_null}},

        {{p_function_call, below},
         {p_null, p_null}},

        {{p_wip_array_access, above},
         {p_primary_expression, below},
         {t_rBracket, cnsme},
         {p_null, p_null}},

        {{p_wip_array_access, above},
         {p_expression, below},
         {t_rBracket, cnsme},
         {p_null, p_null}},

        {{t_reference, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // end
        {{p_null, p_null}},
    },

    // p_wip_array_access
    {
        {{p_primary_expression, above},
         {t_lBracket, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_unary_operator - UNARY-OPERATOR
    {
        // '++'
        // {{t_plus, above},
        //  {t_plus, cnsme},
        //  {p_null, p_null}},

        // '--'
        // {{t_minus, above},
        //  {t_minus, cnsme},
        //  {p_null, p_null}},

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

        // PRIMARY-EXPRESSION UNARY-OPERATOR
        {{p_primary_expression, below},
         {p_unary_operator, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_expression_operator - EXPRESSION-OPERATOR
    {
        {{t_plus, above},
         {p_null, p_null}},

        {{t_minus, above},
         {p_null, p_null}},

        {{t_divide, above},
         {p_null, p_null}},

        {{t_lThan, above},
         {p_null, p_null}},

        {{t_lThanE, above},
         {p_null, p_null}},

        {{t_gThan, above},
         {p_null, p_null}},

        {{t_gThanE, above},
         {p_null, p_null}},

        {{t_equals, above},
         {p_null, p_null}},

        {{t_nEquals, above},
         {p_null, p_null}},

        {{p_null, p_null}}},

    // p_wip_expression - WIP-EXPRESSION
    {
        // PRIMARY-EXPRESSION EXPRESSION-OPERATOR
        {{p_primary_expression, above},
         {p_expression_operator, above},
         {p_null, p_null}},

        // EXPRESSION EXPRESSION-OPERATOR
        {{p_expression, above},
         {p_expression_operator, above},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '*'
        {{p_primary_expression, above},
         {t_star, above},
         {p_null, p_null}},

        // EXPRESSION '*'
        {{p_expression, above},
         {t_star, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_expression - EXPRESSION
    {
        // WIP-EXPRESSION PRIMARY-EXPRESSION
        {{p_wip_expression, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // WIP-EXPRESSION EXPRESSION
        {{p_wip_expression, above},
         {p_expression, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_expression_tail - EXPRESSION-TAIL
    {
        // EXPRESSION ';'
        {{p_expression, above},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION ';'
        {{p_primary_expression, above},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        // WIP-EXPRESSION EXPRESSION-TAIL
        {{p_wip_expression, above},
         {p_expression_tail, below},
         {p_null, p_null}},

        // '*' EXPRESSION-TAIL
        {{t_star, above},
         {p_expression_tail, below},
         {p_null, p_null}},

        // '&' EXPRESSION-TAIL
        {{t_reference, above},
         {p_expression_tail, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_function_opener - FUNCTION-OPENER
    {
        {{t_identifier, above},
         {t_lParen, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_function_call - FUNCTION-CALL
    {
        // FUNCTION-OPENER ')'
        {{p_function_opener, above},
         {t_rParen, below},
         {p_null, p_null}},

        // FUNCTION-OPENER PRIMARY-EXPRESSION ')'
        {{p_function_opener, above},
         {p_primary_expression, below},
         {t_rParen, below},
         {p_null, p_null}},

        // FUNCTION-OPENER EXPRESSION ')'
        {{p_function_opener, above},
         {p_expression, below},
         {t_rParen, below},
         {p_null, p_null}},

        // FUNCTION-OPENER EXPRESSION-LIST ')'
        {{p_function_opener, above},
         {p_expression_list, below},
         {t_rParen, below},
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

    // p_wip_array_declaration - WIP-ARRAY-DECLARATION
    {
        {{p_variable_declaration, above},
         {t_lBracket, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_variable_declaration - VARIABLE-DECLARATION
    {
        // TYPE-NAME PRIMARY-EXPRESSION
        {{p_type_name, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        {{p_wip_array_declaration, above},
         {p_primary_expression, below},
         {t_rBracket, cnsme},
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

        // TYPE-NAME PRIMARY-EXPRESSION ';'
        {{p_type_name, above},
         {p_primary_expression, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_assignment_statement - ASSIGNMENT-STATEMENT
    {
        // VARIABLE-DECLARATION '=' EXPRESSION-TAIL
        {{p_variable_declaration, above},
         {t_single_equals, above},
         {p_expression_tail, below},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION '=' EXPRESSION-TAIL
        {{p_primary_expression, above},
         {t_single_equals, above},
         {p_expression_tail, below},
         {p_null, p_null}},

        {{p_primary_expression, above},
         {t_plus_equals, above},
         {p_expression_tail, below},
         {p_null, p_null}},

        {{p_primary_expression, above},
         {t_minus_equals, above},
         {p_expression_tail, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_return_statement - RETURN-STATEMENT
    {
        // 'return' PRIMARY-EXPRESSION
        {{t_return, above},
         {p_expression_tail, below},
         {p_null, p_null}},

        // 'return' ';'
        {{t_return, above},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // ifs are treated a bit specially to allow stringing together of else-ifs
    // p_if_awating_else - IF-AWAITING-ELSE
    {
        {{p_if, above},
         {t_else, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}}},

    // p_if_else - IF-ELSE
    {
        {{p_if_awating_else, above},
         {p_statement, below},
         {p_null, p_null}},

        {{p_if_awating_else, above},
         {p_scope, below},
         {p_null, p_null}},

        {{p_null, p_null}}},

    // p_if - IF
    {
        // remember that parens around an EXPRESSION becomes a PRIMARY-EXPRESSION
        // 'if' PRIMARY-EXPRESSION SCOPE
        {{t_if, above},
         {p_primary_expression, below},
         {p_scope, below},
         {p_null, p_null}},

        // 'if' PRIMARY-EXPRESSION STATEMENT
        {{t_if, above},
         {p_primary_expression, below},
         {p_statement, below},
         {p_null, p_null}},

        // 'if' '(' PRIMARY-EXPRESSION ')' SCOPE
        {{t_if, above},
         {t_lParen, cnsme},
         {p_primary_expression, below},
         {t_rParen, cnsme},
         {p_scope, below},
         {p_null, p_null}},

        // 'if' '(' PRIMARY-EXPRESSION ')' STATEMENT
        {{t_if, above},
         {t_lParen, cnsme},
         {p_primary_expression, below},
         {t_rParen, cnsme},
         {p_statement, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_statement - STATEMENT
    {
        // VARIABLE-DECLARATION-STATEMENT
        {{p_variable_declaration_statement, above},
         {p_null, p_null}},

        // class declaration
        {{p_type_name, above},
         {p_scope, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        // ASSIGNMENT-STATEMENT
        {{p_assignment_statement, above},
         {p_null, p_null}},

        // only convert to generic statement here so it has lower precedence than binding into an expression
        {{p_expression_tail, above},
         {p_null, p_null}},

        // IF
        {{p_if, above},
         {p_null, p_null}},

        // IF-ELSE
        {{p_if_else, above},
         {p_null, p_null}},

        // WHILE
        {{p_while, above},
         {p_null, p_null}},

        // RETURN-STATEMENT
        {{p_return_statement, above},
         {p_null, p_null}},

        // ASM (autoparsed by scan()) '}' ';'
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

        {{p_statement_list, above},
         {p_scope, besid},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_while - WHILE
    {
        // remember that parens around an EXPRESSION becomes a PRIMARY-EXPRESSION
        // 'while' PRIMARY-EXPRESSION SCOPE
        {{t_while, above},
         {p_primary_expression, below},
         {p_scope, below},
         {p_null, p_null}},

        // 'while' PRIMARY-EXPRESSION STATEMENT
        {{t_while, above},
         {p_primary_expression, below},
         {p_statement, below},
         {p_null, p_null}},

        // 'while' '(' PRIMARY-EXPRESSION ')' SCOPE
        {{t_while, above},
         {t_lParen, cnsme},
         {p_primary_expression, below},
         {t_rParen, cnsme},
         {p_scope, below},
         {p_null, p_null}},

        // 'while' '(' PRIMARY-EXPRESSION ')' STATEMENT
        {{t_while, above},
         {t_lParen, cnsme},
         {p_primary_expression, below},
         {t_rParen, cnsme},
         {p_statement, below},
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

    // p_function_declaration - FUNCTION-DECLARATION
    {
        // multiple arguments
        // 'fun' FUNCTION-OPENER DECLARATION-LIST '->' TYPE-NAME ')'
        {{t_fun, above},
         {p_function_opener, below},
         {p_declaration_list, below},
         {t_arrow, below},
         {p_type_name, below},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // 1 argument
        // 'fun' FUNCTION-OPENER VARIABLE-DECLARATION '->' TYPE-NAME ')'
        {{t_fun, above},
         {p_function_opener, below},
         {p_variable_declaration, below},
         {t_arrow, below},
         {p_type_name, below},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // no arguments
        // 'fun' FUNCTION-OPENER '->' TYPE-NAME ')'
        {{t_fun, above},
         {p_function_opener, below},
         {t_arrow, below},
         {p_type_name, below},
         {t_rParen, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_function_definition - FUNCTION-DEFINITION
    {
        // FUNCTION-DECLARATION SCOPE
        {{p_function_declaration, above},
         {p_scope, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_translation_unit - TRANSLATION-UNIT
    {
        {{p_function_declaration, above},
         {t_semicolon, cnsme},
         {p_null, p_null}},

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

        {{p_statement_list, above},
         {p_translation_unit, besid},
         {p_null, p_null}},

        {{p_null, p_null}}}};
