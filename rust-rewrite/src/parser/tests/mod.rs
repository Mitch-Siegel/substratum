use crate::ast::*;
use crate::lexer::SourceLoc;
#[cfg(test)]
use crate::lexer::Token;
use crate::midend::ir::operands::DualSourceOperands;
use crate::Lexer;
use crate::Parser;
use std::str::Chars;

fn parser_from_string(input: &str) -> Parser<Chars<'_>> {
    Parser::new(Lexer::new(input.chars()))
}

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

#[test]
fn if_statement() {
    let mut p = parser_from_string("if(a > b) {a = a + b;}");
    assert_eq!(
        format!("{}", p.parse_if_statement()),
        "if (a > b)
\t{Compound Statement: a = (a + b)
}"
    );
}

#[test]
fn if_else_statement() {
    let mut p = parser_from_string("if(a > b) {a = a + b;} else {b = b + a;}");
    assert_eq!(
        format!("{}", p.parse_if_statement()),
        "if (a > b)
\t{Compound Statement: a = (a + b)
} else {Compound Statement: b = (b + a)
}"
    );
}
// assert_eq!(p.parse_if_statement(), IfStatementTree {loc: SouceLoc::new(0, 0),
// condition: Expression {loc: SourceLoc::new(0, 5), expression: Expression::Comparison(ComparisonOperationTree::GThan(ArithmeticDualOperands {Box::new(ExpressionTree {loc: SourceLoc::new()})}))}});}}

#[test]
fn while_loop() {
    let mut p = parser_from_string("while (a > b) {b = b + a; count = count + 1;} a = a + count;");
    assert_eq!(
        format!("{}", p.parse_while_loop()),
        "while ((a > b)) Compound Statement: b = (b + a)
count = (count + 1)
"
    );
}
