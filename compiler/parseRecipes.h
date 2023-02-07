#include "ast.h"

// let him cook!
enum token parseRecipes[p_null][4][5] =
    {
        // p_primary_expression
        {{t_identifier, p_null},
         {t_constant, p_null},
         {t_lParen, p_expression, t_rParen, p_null},
         {p_null}},

        // p_binary_expression
        {{p_primary_expression, t_bin_add, p_primary_expression, p_null},
         {p_primary_expression, t_bin_sub, p_primary_expression, p_null},
         {p_null}},

        // p_expression
        {{p_primary_expression, t_assign, p_primary_expression, t_semicolon, p_null},
         {p_null}},

        /*
        {{},
        {p_null}},
        */
};

enum RecipeInstructions
{
    above,
    below,
    cnsme
};

enum RecipeInstructions parseRecipeInstructions[p_null][4][5] =
    {
        // p_primary_expression
        {{above},                // {t_identifier, p_null},
         {above},                // {t_constant, p_null},
         {cnsme, above, cnsme}}, // {t_lParen, p_expression, t_rParen, p_null},

        // p_binary_expression
        {{above, above, below},  // {p_primary_expression, t_bin_add, p_primary_expression, p_null},
         {above, above, below}}, // {p_primary_expression, t_bin_sub, p_primary_expression, p_null},

        // p_expression
        {
            {above, above, below, cnsme, p_null}, // {p_primary_expression, t_assign, p_primary_expression, t_semicolon, p_null},
        },

        /*
        {{},
        {p_null}},
        */
};
