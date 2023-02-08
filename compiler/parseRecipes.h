#include "ast.h"

enum RecipeInstructions
{
    above,
    below,
    besid,
    cnsme
};

// let him cook!
enum token parseRecipes[p_null][6][8][2] = {
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

    // p_binary_expression - BINARY-EXPRESSION
    {
        // PRIMARY-EXPRESSION + PRIMARY-EXPRESSION
        {{p_primary_expression, above},
         {t_bin_add, above},
         {p_primary_expression, below},
         {p_null, p_null}},

        // PRIMARY-EXPRESSION - PRIMARY-EXPRESSION
        {{p_primary_expression, cnsme},
         {t_bin_sub, above},
         {p_primary_expression, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_expression - EXPRESSION
    {
        // PRIMARY-EXPRESSION = PRIMARY-EXPRESSION;
        {{p_primary_expression, above},
         {t_assign, above},
         {p_primary_expression, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_type_specifier - TYPE-SPECIFIER
    {
        // 'void'
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

    // p_parameter_type - PARAMETER-TYPE
    {
        // TYPE-SPECIFIER PRIMARY-EXPRESSION
        {{p_type_specifier, above},
         {p_primary_expression, above},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_declarator - DECLARATOR
    {
        // ()
        // {{t_lParen, cnsme},
        //  {t_rParen, cnsme},
        //  {p_null, p_null}},

        // '[' CONSTANT ']'
        {{t_lBracket, cnsme},
         {t_constant, above},
         {t_rBracket, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_variable_declaration - VARIABLE-DECLARATION
    // this is where it starts to get nasty
    {
        // PARAMETER-TYPE DECLARATOR ';'
        {{p_parameter_type, above},
         {p_declarator, below},
         {t_semicolon, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_function_declaration - FUNCTION-DECLARATION
    {
        // 'fun' TYPE-SPECIFIER '(' ')'
        {{t_fun, above},
         {p_parameter_type, below},
         {t_lParen, cnsme},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // 'fun' PARAMETER-TYPE '(' PARAMETER-TYPE ')'
        {{t_fun, above},
         {p_parameter_type, above},
         {t_lParen, cnsme},
         {p_parameter_type, below},
         {t_rParen, cnsme},
         {p_null, p_null}},

        // 'fun' PARAMETER-TYPE '(' PARAMETER-TYPE-LIST ')'
        {{t_fun, above},
         {p_parameter_type, above},
         {t_lParen, cnsme},
         {p_parameter_type_list, below},
         {t_rParen, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // allow parameters to stack up multiple into one production
    // p_parameter_type_list - PARAMTER-TYPE-LIST
    {
        // PARAMETER-TYPE PARAMETER-TYPE
        {{p_parameter_type, above},
         {p_parameter_type, besid},
         {p_null, p_null}},

        {{p_null, p_null}},
    },

    // p_scope - SCOPE
    {
        // '{' '}'
        {{t_lCurly, cnsme},
         {t_rCurly, cnsme},
         {p_null, p_null}},

        {{p_null, p_null}},
    },
};