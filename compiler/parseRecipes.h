#include "ast.h"

// let him cook!
enum token parseRecipes[p_null][6][7] =
    {
        // p_primary_expression
        {
            {t_identifier, p_null},
            {t_constant, p_null},
            {t_lParen, p_expression, t_rParen, p_null},
            {p_null},
        },

        // p_binary_expression
        {
            {p_primary_expression, t_bin_add, p_primary_expression, p_null},
            {p_primary_expression, t_bin_sub, p_primary_expression, p_null},
            {p_null},
        },

        // p_expression
        {
            {p_primary_expression, t_assign, p_primary_expression, t_semicolon, p_null},
            {p_null},
        },

        // p_type_specifier
        {
            {t_void, p_null},
            {t_uint8, p_null},
            {t_uint16, p_null},
            {t_uint32, p_null},
            {t_void, p_null},
            {p_null},
        },

        // p_parameter_type
        {
            {p_type_specifier, p_primary_expression, p_null},
            {p_null}},

        // p_declarator
        {
            {t_lParen, t_rParen, p_null},
            {t_lBracket, t_constant, t_rBracket, p_null},
            {p_null},
        },

        // p_declaration
        // this is where it starts to get nasty
        {
            {p_parameter_type, p_declarator, t_semicolon, p_null},
            {t_fun, p_type_specifier, p_declarator, t_assign, p_expression, p_null},
            {t_fun, p_type_specifier, t_lParen, p_parameter_type_list, t_rParen, p_scope, p_null},
            {p_null},
        },

        // p_parameter_type_list,
        {
            {p_parameter_type, p_parameter_type, p_null},
            {p_null},
        },

        // p_scope
        {
            {t_lCurly, t_rCurly, p_null},
            {p_null},
        }

        /*
        {{},
        {p_null}},
        */
};

enum RecipeInstructions
{
    above,
    below,
    besid,
    cnsme
};

enum RecipeInstructions parseRecipeInstructions[p_null][6][6] =
    {
        // p_primary_expression
        {
            {above}, // {t_identifier, p_null},
            {above}, // {t_constant, p_null},
            {cnsme, above, cnsme},
        }, // {t_lParen, p_expression, t_rParen, p_null},

        // p_binary_expression
        {
            {above, above, below}, // {p_primary_expression, t_bin_add, p_primary_expression, p_null},
            {above, above, below}, // {p_primary_expression, t_bin_sub, p_primary_expression, p_null},
        },

        // p_expression
        {
            {above, above, below, cnsme}, // {p_primary_expression, t_assign, p_primary_expression, t_semicolon, p_null},
        },

        // p_type_specifier
        {
            {above},
            {above},
            {above},
            {above},
            {above},
        },

        // p_parameter_type
        {
            {above, below},
        },

        // p_declarator
        {
            {cnsme, above},
            {cnsme, above, cnsme},
        },

        // p_declaration
        {
            {above, below, cnsme},
            {above, below, above, below},
            {above, below, cnsme, below, cnsme, below}
        },

        // p_parameter_type_list
        {
            {above, besid},
        },

        // p_scope
        {
            {cnsme, cnsme},
        }

        /*
        {{},
        {p_null}},
        */
};
