#include "ast.h"

enum RecipeInstructions
{
    above,
    below,
    besid,
    cnsme
};

// let him cook!
enum token parseRecipes[p_null][8][8][2] = {

    // p_parameter_decl - PARAMETER-TYPE
    // higher parse precedence than primary expression so we can grab the identifier
    {
        // TYPE-NAME PRIMARY-EXPRESSION
        {{p_type_name, above},
         {t_identifier, above},
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
         {p_assignment_expression, above},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // end
        {{p_null, p_null}},
    },

    // p_postfix_expression
    {
        {{p_primary_expression, above},
         {p_null, p_null}},

        {{p_postfix_expression, above},
         {t_lBracket, cnsme},
         {p_expression, below},
         {t_rBracket, cnsme},
         {p_null, p_null}},

        {{p_postfix_expression, above},
         {t_lParen, cnsme},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // {{p_postfix_expression, above},
        //  {t_lParen, cnsme},
        //  {p_argument_expression_list, below},
        //  {t_rParen, cnsme},
        //  {p_null, p_null}},

        {{p_postfix_expression, above},
         {t_dot, above},
         {t_identifier, below},
         {p_null, p_null}},

        {{p_postfix_expression, above},
         {t_pointer_op, above},
         {t_identifier, below},
         {p_null, p_null}},

        {{p_postfix_expression, above},
         {t_un_inc, below},
         {p_null, p_null}},

        {{p_postfix_expression, above},
         {t_un_inc, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_unary_operator
    {
        {{t_reference, above},
         {p_null, p_null}},

        {{t_star, above},
         {p_null, p_null}},

        {{t_bin_add, above},
         {p_null, p_null}},

        {{t_bin_sub, above},
         {p_null, p_null}},

        {{t_un_bit_not, above},
         {p_null, p_null}},

        {{t_un_log_not, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_unary_expression - UNARY-EXPRESSION
    {
        {{p_postfix_expression, above},
         {p_null, p_null}},

        {{t_un_inc, above},
         {p_unary_expression, below},
         {p_null, p_null}},

        {{t_un_dec, above},
         {p_unary_expression, below},
         {p_null, p_null}},

        {{p_unary_operator, above},
         {p_cast_expression, below},
         {p_null, p_null}},

        /*
        {{t_sizeof, above},
        {p_unary_expression, below},
        {p_null, p_null}},

        {{t_sizeof, above},
        {t_lParen, cnsme},
        {p_type_name, below},
        {t_rParen, cnsme},
        {p_null, p_null}},
        */

        {{p_null, p_null}},
    },

    // p_cast_expression - CAST-EXPRESSION
    {
        {{t_lParen, cnsme},
         {p_type_name, above},
         {t_rParen, cnsme},
         {p_cast_expression, below},
         {p_null, p_null}},

        {{p_unary_expression, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_multiplicative_expression - MULTIPLICATIVE-EXPRESSION
    {
        {{p_multiplicative_expression, above},
         {t_star, above},
         {p_cast_expression, below},
         {p_null, p_null}},

        /*
        {{p_multiplicative_expression, above},
         {t_divide, above},
         {p_cast_expression, below},
         {p_null, p_null}},

        {{p_multiplicative_expression, above},
         {t_modulo, above},
         {p_cast_expression, below},
         {p_null, p_null}},
         */

        {{p_cast_expression, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_additive_expression - ADDITIVE-EXPRESSION
    {
        {{p_additive_expression, above},
         {t_bin_add, above},
         {p_multiplicative_expression, below},
         {p_null, p_null}},

        {{p_additive_expression, above},
         {t_bin_sub, above},
         {p_multiplicative_expression, below},
         {p_null, p_null}},

        {{p_multiplicative_expression, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_shift_expression - SHIFT-EXPRESSION
    {
        // SHIFT-EXPRESSION '<<' ADDITIVE-EXPRESSION
        {{p_shift_expression, above},
         {t_lShift, above},
         {p_additive_expression, below},
         {p_null, p_null}},

        // SHIFT-EXPRESSION '>>' ADDITIVE-EXPRESSION
        {{p_shift_expression, above},
         {t_rShift, above},
         {p_additive_expression, below},
         {p_null, p_null}},

        // ADDITIVE-EXPRESSION
        {{p_additive_expression, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_relational_expression - RELATIONAL-EXPRESSION
    {
        // RELATIONAL-EXPRESSION '<' SHIFT-EXPRESSION
        {{p_relational_expression, above},
         {t_bin_lThan, above},
         {p_shift_expression, below},
         {p_null, p_null}},

        // RELATIONAL-EXPRESSION '>' SHIFT-EXPRESSION
        {{p_relational_expression, above},
         {t_bin_gThan, above},
         {p_shift_expression, below},
         {p_null, p_null}},

        // RELATIONAL-EXPRESSION '<=' SHIFT-EXPRESSION
        {{p_relational_expression, above},
         {t_bin_lThanE, above},
         {p_shift_expression, below},
         {p_null, p_null}},

        // RELATIONAL-EXPRESSION '>=' SHIFT-EXPRESSION
        {{p_relational_expression, above},
         {t_bin_gThanE, above},
         {p_shift_expression, below},
         {p_null, p_null}},

        // SHIFT-EXPRESSION
        {{p_shift_expression, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_equality_expression - EQUALITY-EXPRESSION
    {
        {{p_equality_expression, above}, {t_bin_equals, above}, {p_relational_expression, below}, {p_null, p_null}},

        {{p_equality_expression, above}, {t_bin_notEquals, above}, {p_relational_expression, below}, {p_null, p_null}},

        {{p_relational_expression, above}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_logical_and_expression - LOGICAL-AND-EXPRESSION
    {
        // EQUALITY-EXPRESSION '&&' INCLUSIVE-OR-EXPRESSION
        // higher precedence than a lone EQUALITY-EXPRESSION
        {{p_equality_expression, above}, {t_bin_log_or, above}, {p_logical_and_expression, below}, {p_null, p_null}},

        {{p_equality_expression, above}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_logical_or_expression - LOGICAL-OR-EXPRESSION
    {
        // LOGICAL-OR-EXPRESSION '||' LOGICAL-AND-EXPRESSION
        // higher precedence than a lone LOGICAL-OR-EXPRESSION
        {{p_logical_or_expression, above}, {t_bin_log_or, above}, {p_logical_and_expression, below}, {p_null, p_null}},

        {{p_logical_and_expression, above}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_conditional_expression - CONDITIONAL-EXPRESSION
    {
        // LOGICAL-OR-EXPRESSION '||' LOGICAL-AND-EXPRESSION
        // todo: this will gobble up every logical or expression
        // {{p_logical_or_expression, above},
        //  {p_null, p_null}},

        {{p_logical_or_expression, above}, {t_ternary, above}, {p_expression, below}, {t_colon, cnsme}, {p_conditional_expression, below}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_assignment_expression - ASSIGNMENT-EXPRESSION
    {
        // CONDITIONAL-EXPRESSION;
        {{p_conditional_expression, above}, {p_null, p_null}},

        // UNARY-EXPRESSION '=' ASSIGNMENT-EXPRESSION
        {{p_unary_expression, above},
         {t_assign, above},
         {p_assignment_expression, below},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_type_name - TYPE-NAME
    {
        // 'void'
        {{t_void, above}, {p_null, p_null}},

        // 'uint8'
        {{t_uint8, above}, {p_null, p_null}},

        // 'uint16'
        {{t_uint16, above}, {p_null, p_null}},

        // 'uint32'
        {{t_uint32, above}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_declarator - DECLARATOR
    {
        // ()
        // {{t_lParen, cnsme},
        //  {t_rParen, cnsme},
        //  {p_null, p_null}},

        // '[' CONSTANT ']'
        {{t_lBracket, cnsme}, {t_constant, above}, {t_rBracket, cnsme}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_variable_declaration - VARIABLE-DECLARATION
    // this is where it starts to get nasty
    {
        // PARAMETER-DECL DECLARATOR ';'
        {{p_parameter_decl, above}, {p_declarator, below}, {t_semicolon, cnsme}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_function_declaration - FUNCTION-DECLARATION
    {
        // 'fun' PARAMETER-DECL '(' ')'
        {{t_fun, above}, {p_parameter_decl, below}, {t_lParen, cnsme}, {t_rParen, cnsme}, {p_null, p_null}},

        // 'fun' PARAMETER-DECL '(' PARAMETER-DECL ')'
        {{t_fun, above}, {p_parameter_decl, below}, {t_lParen, cnsme}, {p_parameter_decl, below}, {t_rParen, cnsme}, {p_null, p_null}},

        // 'fun' PARAMETER-DECL '(' PARAMETER-DECL-LIST ')'
        {{t_fun, above}, {p_parameter_decl, below}, {t_lParen, cnsme}, {p_parameter_decl_list, below}, {t_rParen, cnsme}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // allow parameters to stack up multiple into one production
    // p_parameter_decl_list - PARAMTER-DECL-LIST
    {
        // PARAMETER-DECL ',' PARAMETER-DECL
        {{p_parameter_decl, above}, {t_comma, cnsme}, {p_parameter_decl, besid}, {p_null, p_null}},

        // PARAMETER-DECL-LIST ',' PARAMETER-DECL
        {{p_parameter_decl_list, above}, {t_comma, cnsme}, {p_parameter_decl, besid}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_scope - SCOPE
    {
        // '{' '}'
        {{t_lCurly, cnsme}, {t_rCurly, cnsme}, {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_expression
    {
        {{p_assignment_expression, p_null}},

        {{p_null, p_null}},
    },
};