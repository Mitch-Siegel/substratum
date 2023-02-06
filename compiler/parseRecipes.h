#include "ast.h"

enum production
{
    p_primary_expression,
    p_postfix_expression,
    p_argument_expression_list,
    p_unary_expression,
    p_unary_operator,
    p_cast_expression,
    p_multiplicative_expression,
    p_additive_expression,
    p_shift_expression,
    p_relational_expression,
    p_equality_expression,
    p_and_expression,
    p_exclusive_or_expression,
    p_inclusive_or_expression,
    p_logical_and_expression,
    p_logical_or_expression,
    p_conditional_expression,
    p_assignment_expression,
    p_assignment_operator,
    p_expression,
    p_constant_expression,
    p_declaration,
    p_declaration_specifiers,
    p_init_declarator_list,
    p_init_declarator,
    p_storage_class_specifier,
    p_type_specifier,
    p_struct_or_union_specifier,
    p_struct_or_union,
    p_struct_declaration_list,
    p_struct_declaration,
    p_specifier_qualifier_list,
    p_struct_declarator_list,
    p_struct_declarator,
    p_enum_specifier,
    p_enumerator_list,
    p_enumerator,
    p_type_qualifier,
    p_declarator,
    p_direct_declarator,
    p_pointer,
    p_type_qualifier_list,
    p_parameter_type_list,
    p_parameter_list,
    p_parameter_declaration,
    p_identifier_list,
    p_type_name,
    p_abstract_declarator,
    p_direct_abstract_declarator,
    p_initializer,
    p_initializer_list,
    p_statement,
    p_labeled_statement,
    p_compound_statement,
    p_declaration_list,
    p_statement_list,
    p_expression_statement,
    p_selection_statement,
    p_iteration_statement,
    p_jump_statement,
    p_translation_unit,
    p_external_declaration,
    p_function_definition,
    p_null
};

struct ParseIngredient
{
    union
    {
        enum token token;
        enum production production;
    } contents;
    char isProduction;
};

struct ParseIngredient parseRecipes[p_null][9][5] =
    {
        //  p_primary_expression
        {{{t_identifier, 0}, {p_null, 1}},
         {{t_constant, 0}, {p_null, 1}},
         {{t_string_literal, 0}, {p_null, 1}},
         {{t_lParen, 0}, {p_expression, 1}, {t_rParen, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_postfix_expression
        {{{p_primary_expression, 1}, {p_null, 1}},
         {{{p_primary_expression, 1}, {t_lBracket, 0}, {p_expression, 1}, {t_rBracket, 0}, {p_null, 1}},
          {{p_postfix_expression, 1}, {t_lParen, 0}, {t_rParen, 0}, {p_null, 1}},
          {{p_postfix_expression, 1}, {t_lParen, 0}, {p_argument_expression_list, 1}, {t_rParen, 0}, {p_null, 1}},
          {{p_postfix_expression, 1}, {t_dot, 0}, {t_identifier, 0}, {p_null, 1}},
          {{p_postfix_expression, 1}, {t_pointer_op, 0}, {t_identifier, 0}, {p_null, 1}},
          {{p_postfix_expression, 1}, {t_un_inc, 0}, {p_null, 1}},
          {{p_postfix_expression, 1}, {t_un_dec, 0}, {p_null, 1}},
          {{p_null, 1}}},

         // p_argument_expression_list
         {{{p_assignment_expression, 1}, {p_null, 1}},
          {{p_argument_expression_list, 1}, {t_comma, 0}, {p_assignment_expression, 1}, {p_null, 1}},
          {{p_null, 1}}},

         // p_unary_expression
         {{{p_postfix_expression, 1}, {p_null, 1}},
          {{t_un_inc, 0}, {p_unary_expression, 1}, {p_null, 1}},
          {{t_un_dec, 0}, {p_unary_expression, 1}, {p_null, 1}},
          {{p_unary_operator, 1}, {p_cast_expression, 1}, {p_null, 1}},
          {{t_sizeof, 0}, {p_unary_expression, 1}, {p_null, 1}},
          {{t_sizeof, 0}, {t_lParen, 0}, {p_type_name, 1}, {t_rParen, 0}, {p_null, 1}},
          {{p_null, 1}}},

         // p_unary_operator
         {{{t_reference, 0}, {p_null, 1}},
          {{t_dereference, 0}, {p_null, 1}},
          {{t_un_add, 0}, {p_null, 1}},
          {{t_un_sub, 0}, {p_null, 1}},
          {{t_un_bit_not, 0}, {p_null, 1}},
          {{t_un_log_not, 0}, {p_null, 1}},
          {p_null, 1}}},

        // p_cast_expression
        {{{p_unary_expression, 1}, {p_null, 1}},
         {{t_lParen, 0}, {p_type_name, 1}, {t_lParen, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_multiplicative_expression
        {{{p_cast_expression, 1}, {p_null, 1}},
         {{p_multiplicative_expression, 1}, {t_dereference, 0}, {p_null, 1}},
         //  {{p_multiplicative_expression, 1}, {t_div, 0}, {p_null, 1}}, // division not implemented in CPU yet
         //  {{p_multiplicative_expression, 1}, {t_mod, 0}, {p_null, 1}}, // modulo not implemented in CPU yet
         {{p_null, 1}}},

        // p_additive_expression
        {{{p_multiplicative_expression, 1}, {p_null, 1}},
         {{p_additive_expression, 1}, {t_un_add, 0}, {p_null, 1}},
         {{p_additive_expression, 1}, {t_un_sub, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_shift_expression
        {{{p_additive_expression, 1}, {p_null, 1}},
         {{p_shift_expression, 1}, {t_lShift, 0}, {p_additive_expression, 1}, {p_null, 1}},
         {{p_shift_expression, 1}, {t_rShift, 0}, {p_additive_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_relational_expression
        {{{p_shift_expression, 1}, {p_null, 1}},
         {{p_relational_expression, 1}, {t_bin_lThan, 0}, {p_shift_expression, 1}, {p_null, 1}},
         {{p_relational_expression, 1}, {t_bin_gThan, 0}, {p_shift_expression, 1}, {p_null, 1}},
         {{p_relational_expression, 1}, {t_bin_lThanE, 0}, {p_shift_expression, 1}, {p_null, 1}},
         {{p_relational_expression, 1}, {t_bin_gThanE, 0}, {p_shift_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_equality_expression
        {{{p_relational_expression, 1}, {p_null, 1}},
         {{p_equality_expression, 1}, {t_bin_equals, 0}, {p_relational_expression, 1}, {p_null, 1}},
         {{p_equality_expression, 1}, {t_bin_notEquals, 0}, {p_relational_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_and_expression
        {{{p_equality_expression, 1}, {p_null, 1}},
         {{p_and_expression, 1}, {t_reference, 0}, {p_equality_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_exclusive_or_expression
        {{{p_and_expression, 1}, {p_null, 1}},
         {{p_exclusive_or_expression, 1}, {t_un_bit_xor, 0}, {p_and_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_inclusive_or_expression
        {{{p_exclusive_or_expression, 1}, {p_null, 1}},
         {{p_inclusive_or_expression, 1}, {t_un_bit_or, 0}, {p_inclusive_or_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_logical_and_expression
        {{{p_inclusive_or_expression, 1}, {p_null, 1}},
         {{p_logical_and_expression, 1}, {t_bin_log_and, 0}, {p_inclusive_or_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_logical_or_expression
        {{{p_logical_and_expression, 1}, {p_null, 1}},
         {{p_logical_or_expression, 1}, {t_bin_log_or, 0}, {p_logical_and_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_conditional_expression
        {{{p_logical_or_expression, 1}, {p_null, 1}},
         {{p_logical_or_expression, 1}, {t_ternary, 0}, {p_expression, 1}, {t_colon, 0}, {p_conditional_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_assignment_expression
        {{{p_conditional_expression, 1}, {p_null, 1}},
         {{p_unary_expression, 1}, {p_assignment_operator, 1}, {p_assignment_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_assignment_operator
        {{{t_assign, 0}, {p_null, 1}},
         {{t_mul_assign, 0}, {p_null, 1}},
         // {{t_div_assign, 0}, {p_null, 1}}, // division not implemented in CPU yet
         // {{t_mod_assign, 0}, {p_null, 1}}, // modulo not implemented in CPU yet
         {{t_add_assign, 0}, {p_null, 1}},
         {{t_sub_assign, 0}, {p_null, 1}},
         {{t_lshift_assign, 0}, {p_null, 1}},
         {{t_rshift_assign, 0}, {p_null, 1}},
         {{t_bitand_assign, 0}, {p_null, 1}},
         {{t_bitxor_assign, 0}, {p_null, 1}},
         {{t_bitor_assign, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_expression
        {{{p_assignment_expression, 1}, {p_null, 1}},
         {{p_expression, 1}, {t_comma, 0}, {p_assignment_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_constant_expression
        {{{p_conditional_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_declaration
        {{{p_declaration_specifiers, 1}, {t_semicolon, 0}, {p_null, 1}},
         {{p_declaration_specifiers, 1}, {p_init_declarator_list, 1}, {t_semicolon, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_declaration_specifiers
        {{{p_storage_class_specifier, 1}, {p_null, 1}},
         {{p_storage_class_specifier, 1}, {p_null, 1}},
         {{p_type_specifier, 1}, {p_null, 1}},
         {{p_type_specifier, 1}, {p_declaration_specifiers, 1}, {p_null, 1}},
         {{p_type_qualifier, 1}, {p_null, 1}},
         {{p_type_qualifier, 1}, {p_declaration_specifiers, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_init_declarator_list
        {{{p_init_declarator, 1}, {p_null, 1}},
         {{p_init_declarator_list, 1}, {t_comma, 0}, {p_init_declarator, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_init_declarator
        {{{p_declarator, 1}, {p_null, 1}},
         {{p_declarator, 1}, {t_assign, 0}, {p_initializer, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_storage_class_specifier
        {/*{{t_typedef, 0}, {p_null, 1}},*/
         //  {{t_extern, 0}, {p_null, 1}},
         //  {{t_static, 0}, {p_null, 1}},
         //  {{t_auto, 0}, {p_null, 1}},
         //  {{t_register, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_type_specifier
        {{{t_void, 0}, {p_null, 1}},
         {{t_uint8, 0}, {p_null, 1}},
         {{t_uint16, 0}, {p_null, 1}},
         {{t_uint32, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_struct_or_union_specifier
        {{{p_struct_or_union, 1}, {t_identifier, 0}, {t_lCurly, 0}, {p_struct_declaration, 1}, {t_rCurly, 0}, {p_null, 1}},
         {{p_struct_or_union, 1}, {t_identifier, 0}, {t_lCurly, 0}, {p_struct_declaration_list, 1}, {t_rCurly, 0}, {p_null, 1}},
         {{p_struct_or_union, 0}, {t_identifier, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_struct_or_union
        {/*{{t_struct, 0}, {p_null, 1}},*/
         //  {{t_union, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_struct_declaration_list
        {{{p_struct_declaration, 1}, {p_null, 1}},
         {{p_struct_declaration_list, 1}, {p_struct_declaration, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_struct_declaration
        {{{p_specifier_qualifier_list, 1}, {p_struct_declarator_list, 1}, {t_semicolon, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_specifier_qualifier_list
        {{{p_type_specifier, 1}, {p_specifier_qualifier_list, 1}, {p_null, 1}},
         {{p_type_specifier, 1}, {p_null, 1}},
         {{p_type_qualifier, 1}, {p_specifier_qualifier_list, 1}, {p_null, 1}},
         {{p_type_qualifier, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_struct_declarator_list
        {{{p_struct_declarator, 1}, {p_null, 1}},
         {{p_struct_declarator_list, 1}, {t_comma, 0}, {p_struct_declarator, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_struct_declarator
        {{{p_declarator, 1}, {p_null, 1}},
         {{t_colon, 0}, {p_constant_expression, 1}, {p_null, 1}},
         {{p_declarator, 1}, {t_colon, 0}, {p_constant_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_enum_specifier
        {/*{{t_enum, 0}, {t_lCurly, 0}, {p_enumerator_list, 1}, {t_rCurly, 0}, {p_null, 1}},
         {{t_enum, 0}, {t_identifier, 1}, {t_lCurly, 0}, {p_enumerator_list, 1}, {t_rCurly, 0}, {p_null, 1}},
         {{t_enum, 0}, {t_identifier, 1}, {p_null, 1}},*/
         {{p_null, 1}}},

        // p_enumerator_list
        {{{p_enumerator, 1}, {p_null, 1}},
         {{p_enumerator_list, 1}, {t_comma, 0}, {p_enumerator, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_enumerator
        {{{t_identifier, 0}, {p_null, 1}},
         {{t_identifier, 0}, {t_assign, 0}, {p_constant_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_type_qualifier
        {/*{{t_const, 0}, {p_null, 1}},
        {{t_volatile, 0}, {p_null, 1}},*/
         {{p_null, 1}}},

        // p_declarator
        {{{p_pointer, 1}, {p_direct_declarator, 1}, {p_null, 1}},
         {{p_direct_declarator, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_direct_declarator
        {{{t_identifier, 0}, {p_null, 1}},
         {{t_lParen, 0}, {p_declarator, 1}, {t_rParen, 0}, {p_null, 1}},
         {{p_direct_declarator, 1}, {t_lBracket, 0}, {p_constant_expression, 1}, {t_rBracket, 0}, {p_null, 1}},
         {{p_direct_declarator, 1}, {t_lBracket, 0}, {t_rBracket, 0}, {p_null, 1}},
         {{p_direct_declarator, 1}, {t_lParen, 0}, {p_parameter_type_list, 1}, {t_rParen, 0}, {p_null, 1}},
         {{p_direct_declarator, 1}, {t_lParen, 0}, {p_identifier_list, 1}, {t_rParen, 0}, {p_null, 1}},
         {{p_direct_declarator, 1}, {t_lParen, 0}, {t_rParen, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_pointer
        {{{t_dereference, 0}, {p_null, 1}},
         {{t_dereference, 0}, {p_type_qualifier_list, 1}, {p_null, 1}},
         {{t_dereference, 0}, {p_pointer, 1}},
         {{t_dereference, 0}, {p_type_qualifier_list, 1}, {p_pointer, 0}, {p_null, 1}},
         {{t_dereference, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_type_qualifier_list
        {{{p_type_qualifier, 1}, {p_null, 1}},
         {{p_type_qualifier_list, 1}, {p_type_qualifier, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_parameter_type_list
        {{{p_parameter_list, 1}, {p_null, 1}},
         //  {{p_parameter_list, 1}, {t_comma, 0}, {t_elipsis, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_parameter_list
        {{{p_parameter_declaration, 1}, {p_null, 1}},
         {{p_parameter_list, 1}, {t_comma, 0}, {p_parameter_declaration, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_parameter_declaration
        {{{p_declaration_specifiers, 1}, {p_declarator, 1}, {p_null, 1}},
         {{p_declaration_specifiers, 1}, {p_abstract_declarator, 1}, {p_null, 1}},
         {{p_declaration_specifiers, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_identifier_list
        {{{t_identifier, 0}, {p_null, 1}},
         {{p_identifier_list, 1}, {t_comma, 0}, {t_identifier, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_type_name
        {{{p_specifier_qualifier_list, 1}, {p_null, 1}},
         {{p_specifier_qualifier_list, 1}, {p_abstract_declarator, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_abstract_declarator
        {{{p_pointer, 1}, {p_null, 1}},
         {{p_direct_abstract_declarator, 1}, {p_null, 1}},
         {{p_pointer, 1}, {p_direct_abstract_declarator, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_direct_abstract_declarator
        {{{t_lBracket, 0}, {p_abstract_declarator, 1}, {t_rBracket, 0}, {p_null, 1}},
         {{t_lBracket, 0}, {t_rBracket, 0}, {p_null, 1}},
         {{t_lBracket, 0}, {p_constant_expression, 1}, {t_rBracket, 0}, {p_null, 1}},
         {{p_direct_abstract_declarator, 1}, {t_lBracket, 0}, {t_rBracket, 1}, {p_null, 1}},
         {{p_direct_abstract_declarator, 1}, {t_lBracket, 0}, {p_constant_expression, 1}, {t_rBracket, 1}, {p_null, 1}},
         {{t_lParen, 0}, {t_rParen, 0}, {p_null, 1}},
         {{t_lParen, 0}, {p_parameter_type_list, 1}, {t_rParen, 0}, {p_null, 1}},
         {{p_direct_abstract_declarator, 1}, {t_lParen, 0}, {t_rParen, 0}, {p_null, 1}},
         {{p_direct_abstract_declarator, 1}, {t_lParen, 0}, {p_parameter_type_list, 1}, {t_rParen, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_initializer
        {{{p_assignment_expression, 1}, {p_null, 1}},
         {{t_lCurly, 0}, {p_initializer_list, 1}, {t_rCurly, 0}, {p_null, 1}},
         {{t_lCurly, 0}, {p_initializer_list, 1}, {t_comma}, {t_rCurly, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_initializer_list
        {{{p_initializer, 1}, {p_null, 1}},
         {{p_initializer_list, 1}, {t_comma, 0}, {p_initializer, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_statement
        {{{p_labeled_statement, 1}, {p_null, 1}},
         {{p_compound_statement, 1}, {p_null, 1}},
         {{p_expression_statement, 1}, {p_null, 1}},
         {{p_selection_statement, 1}, {p_null, 1}},
         {{p_iteration_statement, 1}, {p_null, 1}},
         {{p_jump_statement, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_labeled_statement
        {{{t_identifier, 0}, {t_colon, 0}, {p_statement, 1}, {p_null, 1}},
         //  {{t_case, 0}, {p_constant_expression, 0}, {t_colon, 0}, {p_statement, 1}, {p_null, 1}},
         //  {{t_default, 0}, {t_colon, 0}, {p_statement, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_compound_statement
        {{{t_lCurly, 0}, {t_rCurly, 0}, {p_null, 1}},
         {{t_lCurly, 0}, {p_statement_list, 1}, {t_rCurly, 0}, {p_null, 1}},
         {{t_lCurly, 0}, {p_declaration_list, 1}, {t_rCurly, 0}, {p_null, 1}},
         {{t_lCurly, 0}, {p_declaration_list, 1}, {p_statement_list, 1}, {t_rCurly, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_declaration_list
        {{{p_declaration, 1}, {p_null, 1}},
         {{p_declaration_list, 1}, {p_declaration, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_statement_list
        {{{p_statement, 1}, {p_null, 1}},
         {{p_statement_list, 1}, {p_statement, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_expression_statement
        {{{t_semicolon, 0}, {p_null, 1}},
         {{p_expression, 1}, {t_semicolon, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_selection_statement
        {{{t_if, 0}, {t_lParen, 0}, {p_expression, 1}, {t_rParen, 0}, {p_null, 1}},
         {{t_if, 0}, {t_lParen, 0}, {p_expression, 1}, {t_rParen, 0}, {p_statement, 1}, {t_else, 0}, {p_statement, 1}, {p_null, 1}},
         //  {{t_switch, 0}, {t_lParen, 0}, {p_expression, 1}, {t_rParen, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_iteration_statement
        {{{t_while, 0}, {t_lParen, 0}, {p_expression, 1}, {t_rParen, 0}, {p_statement}, {p_null, 1}},
         {{t_do, 0}, {p_statement}, {t_while}, {t_lParen, 0}, {p_expression, 1}, {t_rParen, 0}, {p_null, 1}},
         {{t_for, 0}, {t_lParen, 0}, {p_expression, 1}, {p_expression, 1}, {t_rParen, 0}, {p_null, 1}},
         {{t_for, 0}, {t_lParen, 0}, {p_expression, 1}, {p_expression, 1}, {p_expression, 2}, {t_rParen, 0}, {p_null, 1}},
         {{p_null, 1}}},

        // p_jump_statement
        {/*{{t_goto, 0}, {t_identifier, 0}, {t_semicolon, 0}, {p_null, 1}},*/
         //  {{t_continue, 0}, {t_semicolon, 0}, {p_null, 1}},
         //  {{t_break, 0}, {t_semicolon, 0}, {p_null, 1}},
         //  {{t_return, 0}, {t_semicolon, 0}, {p_null, 1}},
         {{t_return, 0}, {p_expression, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_translation_unit
        {{{p_external_declaration, 1}, {p_null, 1}},
         {{p_translation_unit, 1}, {p_external_declaration, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_external_declaration
        {{{p_function_definition, 1}, {p_null, 1}},
         {{p_declaration, 1}, {p_null, 1}},
         {{p_null, 1}}},

        // p_function_definition

        {{{p_declaration_specifiers, 1}, {p_declarator, 1}, {p_declaration_list, 1}, {p_compound_statement, 1}, {p_null, 1}},
         {{p_declaration_specifiers, 1}, {p_declarator, 1}, {p_compound_statement, 1}, {p_null, 1}},
         {{p_declarator, 1}, {p_declaration_list, 1}, {p_compound_statement, 1}, {p_null, 1}},
         {{p_declarator, 1}, {p_compound_statement, 1}, {p_null, 1}},
         {{p_null, 1}}}};
