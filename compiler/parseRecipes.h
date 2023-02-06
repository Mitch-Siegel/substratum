#include "ast.h"

enum token parseRecipes[p_null][10][8] =
    {
        //  p_primary_expression
        {{t_identifier, p_null},
         {t_constant, p_null},
         {t_string_literal, p_null},
         {t_lParen, p_expression, t_rParen, p_null},
         {p_null}},

        // p_postfix_expression
        {{p_primary_expression, p_null},
         {p_primary_expression, t_lBracket, p_expression, t_rBracket, p_null},
         {p_postfix_expression, t_lParen, t_rParen, p_null},
         {p_postfix_expression, t_lParen, p_argument_expression_list, t_rParen, p_null},
         {p_postfix_expression, t_dot, t_identifier, p_null},
         {p_postfix_expression, t_pointer_op, t_identifier, p_null},
         {p_postfix_expression, t_un_inc, p_null},
         {p_postfix_expression, t_un_dec, p_null},
         {p_null}},

        // p_argument_expression_list
        {{p_assignment_expression, p_null},
         {p_argument_expression_list, t_comma, p_assignment_expression, p_null},
         {p_null}},

        // p_unary_expression
        {{p_postfix_expression, p_null},
         {t_un_inc, p_unary_expression, p_null},
         {t_un_dec, p_unary_expression, p_null},
         {p_unary_operator, p_cast_expression, p_null},
         {t_sizeof, p_unary_expression, p_null},
         {t_sizeof, t_lParen, p_type_name, t_rParen, p_null},
         {p_null}},

        // p_unary_operator
        {{t_reference, p_null},
         {t_dereference, p_null},
         {t_un_add, p_null},
         {t_un_sub, p_null},
         {t_un_bit_not, p_null},
         {t_un_log_not, p_null},
         {p_null}},

        // p_cast_expression
        {{p_unary_expression, p_null},
         {t_lParen, p_type_name, t_lParen, p_null},
         {p_null}},

        // p_multiplicative_expression
        {{p_cast_expression, p_null},
         {p_multiplicative_expression, t_dereference, p_null},
         //  {p_multiplicative_expression, t_div, p_null}, // division not implemented in CPU yet
         //  {p_multiplicative_expression, t_mod, p_null}, // modulo not implemented in CPU yet
         {p_null}},

        // p_additive_expression
        {{p_multiplicative_expression, p_null},
         {p_additive_expression, t_un_add, p_null},
         {p_additive_expression, t_un_sub, p_null},
         {p_null}},

        // p_shift_expression
        {{p_additive_expression, p_null},
         {p_shift_expression, t_lShift, p_additive_expression, p_null},
         {p_shift_expression, t_rShift, p_additive_expression, p_null},
         {p_null}},

        // p_relational_expression
        {{p_shift_expression, p_null},
         {p_relational_expression, t_bin_lThan, p_shift_expression, p_null},
         {p_relational_expression, t_bin_gThan, p_shift_expression, p_null},
         {p_relational_expression, t_bin_lThanE, p_shift_expression, p_null},
         {p_relational_expression, t_bin_gThanE, p_shift_expression, p_null},
         {p_null}},

        // p_equality_expression
        {{p_relational_expression, p_null},
         {p_equality_expression, t_bin_equals, p_relational_expression, p_null},
         {p_equality_expression, t_bin_notEquals, p_relational_expression, p_null},
         {p_null}},

        // p_and_expression
        {{p_equality_expression, p_null},
         {p_and_expression, t_reference, p_equality_expression, p_null},
         {p_null}},

        // p_exclusive_or_expression
        {{p_and_expression, p_null},
         {p_exclusive_or_expression, t_un_bit_xor, p_and_expression, p_null},
         {p_null}},

        // p_inclusive_or_expression
        {{p_exclusive_or_expression, p_null},
         {p_inclusive_or_expression, t_un_bit_or, p_inclusive_or_expression, p_null},
         {p_null}},

        // p_logical_and_expression
        {{p_inclusive_or_expression, p_null},
         {p_logical_and_expression, t_bin_log_and, p_inclusive_or_expression, p_null},
         {p_null}},

        // p_logical_or_expression
        {{p_logical_and_expression, p_null},
         {p_logical_or_expression, t_bin_log_or, p_logical_and_expression, p_null},
         {p_null}},

        // p_conditional_expression
        {{p_logical_or_expression, p_null},
         {p_logical_or_expression, t_ternary, p_expression, t_colon, p_conditional_expression, p_null},
         {p_null}},

        // p_assignment_expression
        {{p_conditional_expression, p_null},
         {p_unary_expression, p_assignment_operator, p_assignment_expression, p_null},
         {p_null}},

        // p_assignment_operator
        {{t_assign, p_null},
         {t_mul_assign, p_null},
         // {t_div_assign, p_null}, // division not implemented in CPU yet
         // {t_mod_assign, p_null}, // modulo not implemented in CPU yet
         {t_add_assign, p_null},
         {t_sub_assign, p_null},
         {t_lshift_assign, p_null},
         {t_rshift_assign, p_null},
         {t_bitand_assign, p_null},
         {t_bitxor_assign, p_null},
         {t_bitor_assign, p_null},
         {p_null}},

        // p_expression
        {{p_assignment_expression, p_null},
         {p_expression, t_comma, p_assignment_expression, p_null},
         {p_null}},

        // p_constant_expression
        {{p_conditional_expression, p_null},
         {p_null}},

        // p_declaration
        {{p_declaration_specifiers, t_semicolon, p_null},
         {p_declaration_specifiers, p_init_declarator_list, t_semicolon, p_null},
         {p_null}},

        // p_declaration_specifiers
        {{p_storage_class_specifier, p_null},
         {p_storage_class_specifier, p_null},
         {p_type_specifier, p_null},
         {p_type_specifier, p_declaration_specifiers, p_null},
         {p_type_qualifier, p_null},
         {p_type_qualifier, p_declaration_specifiers, p_null},
         {p_null}},

        // p_init_declarator_list
        {{p_init_declarator, p_null},
         {p_init_declarator_list, t_comma, p_init_declarator, p_null},
         {p_null}},

        // p_init_declarator
        {{p_declarator, p_null},
         {p_declarator, t_assign, p_initializer, p_null},
         {p_null}},

        // p_storage_class_specifier
        {/*{t_typedef, p_null},*/
         //  {t_extern, p_null},
         //  {t_static, p_null},
         //  {t_auto, p_null},
         //  {t_register, p_null},
         {p_null}},

        // p_type_specifier
        {{t_void, p_null},
         {t_uint8, p_null},
         {t_uint16, p_null},
         {t_uint32, p_null},
         {p_null}},

        // p_struct_or_union_specifier
        {{p_struct_or_union, t_identifier, t_lCurly, p_struct_declaration, t_rCurly, p_null},
         {p_struct_or_union, t_identifier, t_lCurly, p_struct_declaration_list, t_rCurly, p_null},
         {p_struct_or_union, t_identifier, p_null},
         {p_null}},

        // p_struct_or_union
        {
            /*{t_struct, p_null},*/
            //  {t_union, p_null},
            {p_null}},

        // p_struct_declaration_list
        {{p_struct_declaration, p_null},
         {p_struct_declaration_list, p_struct_declaration, p_null},
         {p_null}},

        // p_struct_declaration
        {{p_specifier_qualifier_list, p_struct_declarator_list, t_semicolon, p_null},
         {p_null}},

        // p_specifier_qualifier_list
        {{p_type_specifier, p_specifier_qualifier_list, p_null},
         {p_type_specifier, p_null},
         {p_type_qualifier, p_specifier_qualifier_list, p_null},
         {p_type_qualifier, p_null},
         {p_null}},

        // p_struct_declarator_list
        {{p_struct_declarator, p_null},
         {p_struct_declarator_list, t_comma, p_struct_declarator, p_null},
         {p_null}},

        // p_struct_declarator
        {{p_declarator, p_null},
         {t_colon, p_constant_expression, p_null},
         {p_declarator, t_colon, p_constant_expression, p_null},
         {p_null}},

        // p_enum_specifier
        {/*{t_enum, t_lCurly, p_enumerator_list, t_rCurly, p_null},
            {t_enum, t_identifier, t_lCurly, p_enumerator_list, t_rCurly, p_null},
            {t_enum, t_identifier, p_null},*/
         {p_null}},

        // p_enumerator_list
        {{p_enumerator, p_null},
         {p_enumerator_list, t_comma, p_enumerator, p_null},
         {p_null}},

        // p_enumerator
        {{t_identifier, p_null},
         {t_identifier, t_assign, p_constant_expression, p_null},
         {p_null}},

        // p_type_qualifier
        {/*{t_const, p_null},
        {t_volatile, p_null},*/
         {p_null}},

        // p_declarator
        {{p_pointer, p_direct_declarator, p_null},
         {p_direct_declarator, p_null},
         {p_null}},

        // p_direct_declarator
        {{t_identifier, p_null},
         {t_lParen, p_declarator, t_rParen, p_null},
         {p_direct_declarator, t_lBracket, p_constant_expression, t_rBracket, p_null},
         {p_direct_declarator, t_lBracket, t_rBracket, p_null},
         {p_direct_declarator, t_lParen, p_parameter_type_list, t_rParen, p_null},
         {p_direct_declarator, t_lParen, p_identifier_list, t_rParen, p_null},
         {p_direct_declarator, t_lParen, t_rParen, p_null},
         {p_null}},

        // p_pointer
        {{t_dereference, p_null},
         {t_dereference, p_type_qualifier_list, p_null},
         {t_dereference, p_pointer},
         {t_dereference, p_type_qualifier_list, p_pointer, p_null},
         {t_dereference, p_null},
         {p_null}},

        // p_type_qualifier_list
        {{p_type_qualifier, p_null},
         {p_type_qualifier_list, p_type_qualifier, p_null},
         {p_null}},

        // p_parameter_type_list
        {{p_parameter_list, p_null},
         //  {p_parameter_list, t_comma, t_elipsis, p_null},
         {p_null}},

        // p_parameter_list
        {{p_parameter_declaration, p_null},
         {p_parameter_list, t_comma, p_parameter_declaration, p_null},
         {p_null}},

        // p_parameter_declaration
        {{p_declaration_specifiers, p_declarator, p_null},
         {p_declaration_specifiers, p_abstract_declarator, p_null},
         {p_declaration_specifiers, p_null},
         {p_null}},

        // p_identifier_list
        {{t_identifier, p_null},
         {p_identifier_list, t_comma, t_identifier, p_null},
         {p_null}},

        // p_type_name
        {{p_specifier_qualifier_list, p_null},
         {p_specifier_qualifier_list, p_abstract_declarator, p_null},
         {p_null}},

        // p_abstract_declarator
        {{p_pointer, p_null},
         {p_direct_abstract_declarator, p_null},
         {p_pointer, p_direct_abstract_declarator, p_null},
         {p_null}},

        // p_direct_abstract_declarator
        {{t_lBracket, p_abstract_declarator, t_rBracket, p_null},
         {t_lBracket, t_rBracket, p_null},
         {t_lBracket, p_constant_expression, t_rBracket, p_null},
         {p_direct_abstract_declarator, t_lBracket, t_rBracket, p_null},
         {p_direct_abstract_declarator, t_lBracket, p_constant_expression, t_rBracket, p_null},
         {t_lParen, t_rParen, p_null},
         {t_lParen, p_parameter_type_list, t_rParen, p_null},
         {p_direct_abstract_declarator, t_lParen, t_rParen, p_null},
         {p_direct_abstract_declarator, t_lParen, p_parameter_type_list, t_rParen, p_null},
         {p_null}},

        // p_initializer
        {{p_assignment_expression, p_null},
         {t_lCurly, p_initializer_list, t_rCurly, p_null},
         {t_lCurly, p_initializer_list, t_comma},
         {t_rCurly, p_null},
         {p_null}},

        // p_initializer_list
        {{p_initializer, p_null},
         {p_initializer_list, t_comma, p_initializer, p_null},
         {p_null}},

        // p_statement
        {{p_labeled_statement, p_null},
         {p_compound_statement, p_null},
         {p_expression_statement, p_null},
         {p_selection_statement, p_null},
         {p_iteration_statement, p_null},
         {p_jump_statement, p_null},
         {p_null}},

        // p_labeled_statement
        {{t_identifier, t_colon, p_statement, p_null},
         //  {t_case, p_constant_expression, t_colon, p_statement, p_null},
         //  {t_default, t_colon, p_statement, p_null},
         {p_null}},

        // p_compound_statement
        {{t_lCurly, t_rCurly, p_null},
         {t_lCurly, p_statement_list, t_rCurly, p_null},
         {t_lCurly, p_declaration_list, t_rCurly, p_null},
         {t_lCurly, p_declaration_list, p_statement_list, t_rCurly, p_null},
         {p_null}},

        // p_declaration_list
        {{p_declaration, p_null},
         {p_declaration_list, p_declaration, p_null},
         {p_null}},

        // p_statement_list
        {{p_statement, p_null},
         {p_statement_list, p_statement, p_null},
         {p_null}},

        // p_expression_statement
        {{t_semicolon, p_null},
         {p_expression, t_semicolon, p_null},
         {p_null}},

        // p_selection_statement
        {{t_if, t_lParen, p_expression, t_rParen, p_null},
         {t_if, t_lParen, p_expression, t_rParen, p_statement, t_else, p_statement, p_null},
         //  {t_switch, t_lParen, p_expression, t_rParen, p_null},
         {p_null}},

        // p_iteration_statement
        {{t_while, t_lParen, p_expression, t_rParen, p_statement, p_null},
         {t_do, p_statement},
         {t_while},
         {t_lParen, p_expression, t_rParen, p_null},
         {t_for, t_lParen, p_expression, p_expression, t_rParen, p_null},
         {t_for, t_lParen, p_expression, p_expression, p_expression, 2},
         {t_rParen, p_null},
         {p_null}},

        // p_jump_statement
        {/*{t_goto, t_identifier, t_semicolon, p_null},*/
         //  {t_continue, t_semicolon, p_null},
         //  {t_break, t_semicolon, p_null},
         //  {t_return, t_semicolon, p_null},
         {t_return, p_expression, p_null},
         {p_null}},

        // p_translation_unit
        {{p_external_declaration, p_null},
         {p_translation_unit, p_external_declaration, p_null},
         {p_null}},

        // p_external_declaration
        {{p_function_definition, p_null},
         {p_declaration, p_null},
         {p_null}},

        // p_function_definition

        {{p_declaration_specifiers, p_declarator, p_declaration_list, p_compound_statement, p_null},
         {p_declaration_specifiers, p_declarator, p_compound_statement, p_null},
         {p_declarator, p_declaration_list, p_compound_statement, p_null},
         {p_declarator, p_compound_statement, p_null},
         {p_null}}};
