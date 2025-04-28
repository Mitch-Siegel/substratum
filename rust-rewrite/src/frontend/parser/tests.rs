use crate::frontend::ast::*;
use crate::frontend::lexer::{token::Token, *};
use crate::frontend::sourceloc::SourceLoc;
use crate::midend::types::Type;
use crate::Parser;

#[cfg(test)]
fn parser_from_string<'a>(input: &'a str) -> Parser<'a> {
    Parser::new(Lexer::from_string(input))
}

/// Expressions
#[cfg(test)]
fn parse_and_print_expression(input: &str) -> String {
    let mut parser = parser_from_string(input);
    let expr_string = parser.parse_expression().to_string();
    parser.expect_token(Token::Eof);
    expr_string
}

#[test]
fn basic_expression() {
    assert_eq!(
        parse_and_print_expression("123 + 456 + 789"),
        "(123 + (456 + 789))"
    );
}

#[test]
fn addition_and_multiplication() {
    assert_eq!(
        parse_and_print_expression("123 + 456 * 789"),
        "(123 + (456 * 789))"
    );
}

#[test]
fn parentheses_override_precedence() {
    assert_eq!(
        parse_and_print_expression("(123 + 456) * 789"),
        "((123 + 456) * 789)"
    );
}

#[test]
fn mixed_arithmetic_operations() {
    assert_eq!(
        parse_and_print_expression("1 + 2 * 3 - 4 / 5"),
        "(1 + ((2 * 3) - (4 / 5)))"
    );
}

#[test]
fn nested_parentheses() {
    assert_eq!(
        parse_and_print_expression("((1 + 2) * (3 - 4)) / 5"),
        "(((1 + 2) * (3 - 4)) / 5)"
    );
}

#[test]
fn single_number() {
    assert_eq!(parse_and_print_expression("42"), "42");
}

#[test]
fn single_number_parenthesized() {
    assert_eq!(parse_and_print_expression("(42)"), "42");
}

#[test]
fn multiple_additions() {
    assert_eq!(parse_and_print_expression("1 + 2 + 3"), "(1 + (2 + 3))");
}

#[test]
fn complex_arithmetic_expression() {
    assert_eq!(
        parse_and_print_expression("3 + 4 * 2 / (1 - 5)"),
        "(3 + (4 * (2 / (1 - 5))))"
    );
}

/// variable declarations
#[cfg(test)]
fn parse_and_print_variable_declaration(input: &str) -> String {
    let mut parser = parser_from_string(input);
    let ident = parser.parse_identifier();
    let expr_string = parser.parse_variable_declaration(ident).to_string();
    parser.expect_token(Token::Eof);
    expr_string
}

#[test]
fn u8_declaration() {
    assert_eq!(parse_and_print_variable_declaration("abc: u8"), "abc: u8");
}

#[test]
fn if_expression() {
    let mut p = parser_from_string("if(a > b) {a = a + b;}");
    assert_eq!(
        format!("{}", p.parse_if_expression()),
        "if (a > b)
\t{Compound Expression: a = (a + b)
}"
    );
}

#[test]
fn if_else_expression() {
    let mut p = parser_from_string("if(a > b) {a = a + b;} else {b = b + a;}");
    assert_eq!(
        format!("{}", p.parse_if_expression()),
        "if (a > b)
\t{Compound Expression: a = (a + b)
} else {Compound Expression: b = (b + a)
}"
    );
}

#[test]
fn while_loop() {
    let mut p = parser_from_string("while (a > b) {b = b + a; count = count + 1;} a = a + count;");
    assert_eq!(
        format!("{}", p.parse_while_expression()),
        "while ((a > b)) Compound Expression: b = (b + a)
count = (count + 1)
"
    );
}

#[test]
fn struct_definition() {
    let mut p = parser_from_string("struct money{\ndollars: u64,\ncents: u8}");

    assert_eq!(
        p.parse_struct_definition(),
        TranslationUnit::StructDefinition(StructDefinitionTree {
            loc: SourceLoc::new(1, 2),
            name: "money".into(),
            fields: vec![
                VariableDeclarationTree {
                    loc: SourceLoc::new(2, 13),
                    name: "dollars".into(),
                    typename: TypenameTree {
                        loc: SourceLoc::new(2, 14),
                        type_: Type::U64
                    }
                },
                VariableDeclarationTree {
                    loc: SourceLoc::new(3, 10),
                    name: "cents".into(),
                    typename: TypenameTree {
                        loc: SourceLoc::new(3, 11),
                        type_: Type::U8
                    }
                }
            ]
        })
    )
}
