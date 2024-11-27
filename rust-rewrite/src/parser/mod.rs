use crate::ast::*;
use crate::lexer::*;

pub struct Parser<I>
where
    I: Iterator<Item = char>,
{
    lexer: Lexer<I>,
}

enum OperatorPrecedences {
    Add,
    Subtract,
    Multiply,
    Divide,
}

impl OperatorPrecedences {
    pub fn get(&self) -> usize {
        match self {
            Self::Add => 1,
            Self::Subtract => 1,
            Self::Multiply => 2,
            Self::Divide => 2,
        }
    }

    pub fn of_token(token: &Token) -> usize {
        match token {
            Token::Plus => Self::Add.get(),
            Token::Minus => Self::Subtract.get(),
            Token::Star => Self::Multiply.get(),
            Token::FSlash => Self::Divide.get(),
            _ => {
                panic!(
                    "Invalid token {} passed to OperatorPrecedences::of_token()",
                    token
                );
            }
        }
    }
}

impl<I> Parser<I>
where
    I: Iterator<Item = char>,
{
    pub fn new(lexer: Lexer<I>) -> Self
    where
        I: Iterator<Item = char>,
    {
        Parser { lexer: lexer }
    }

    fn peek_token(&self) -> Token {
        return self.lexer.peek();
    }

    fn next_token(&mut self) -> Token {
        return self.lexer.next();
    }

    fn expect_token(&mut self, t: Token) -> Token {
        if self.peek_token().eq(&t) {
            self.next_token()
        } else {
            panic!(
                "Expected token {} at {}, got token {} instead!",
                t,
                self.lexer.current_loc(),
                self.peek_token()
            );
        }
    }

    fn current_loc(&self) -> SourceLoc {
        self.lexer.current_loc()
    }

    fn unexpected_token<T>(&self) -> T {
        panic!(
            "Unexpected token {} at {}",
            self.peek_token(),
            self.current_loc()
        );
    }

    pub fn parse(&mut self) -> Vec<TranslationUnitTree> {
        let mut translation_units = Vec::new();
        while self.lexer.peek() != Token::Eof {
            translation_units.push(self.parse_translation_unit());
        }
        translation_units
    }

    pub fn parse_translation_unit(&mut self) -> TranslationUnitTree {
        match self.peek_token() {
            Token::Fun => self.parse_function_declaration_or_definition(),
            _ => self.unexpected_token::<TranslationUnitTree>(),
        }
    }

    pub fn parse_function_declaration_or_definition(&mut self) -> TranslationUnitTree {
        let function_declaration = self.parse_function_prototype();
        match self.peek_token() {
            Token::LCurly => TranslationUnitTree {
                loc: function_declaration.loc,
                contents: TranslationUnit::FunctionDefinition(FunctionDefinitionTree {
                    prototype: function_declaration,
                    body: self.parse_compoound_statement(),
                }),
            },
            _ => TranslationUnitTree {
                loc: function_declaration.loc,
                contents: TranslationUnit::FunctionDeclaration(function_declaration),
            },
        }
    }

    pub fn parse_compoound_statement(&mut self) -> CompoundStatementTree {
        let start_loc = self.current_loc();
        self.expect_token(Token::LCurly);
        let mut statements: Vec<StatementTree> = Vec::new();
        loop {
            match self.peek_token() {
                Token::RCurly => break,
                _ => statements.push(self.parse_statement()),
            }
        }
        self.expect_token(Token::RCurly);
        CompoundStatementTree {
            loc: start_loc,
            statements: statements,
        }
    }

    pub fn parse_statement(&mut self) -> StatementTree {
        let statement_tree = StatementTree {
            loc: self.current_loc(),
            statement: match self.peek_token() {
                Token::U8 | Token::U16 | Token::U32 | Token::U64 => {
                    Statement::VariableDeclaration(self.parse_variable_declaration())
                }
                Token::Identifier(_) => Statement::Assignment(self.parse_assignment()),
                _ => self.unexpected_token(),
            },
        };
        self.expect_token(Token::Semicolon);
        statement_tree
    }

    pub fn parse_assignment(&mut self) -> AssignmentTree {
        let start_loc = self.current_loc();
        let lhs = self.parse_identifier();
        self.expect_token(Token::Assign);
        AssignmentTree {
            loc: start_loc,
            identifier: lhs,
            value: self.parse_expression(),
        }
    }

    pub fn parse_primary_expression(&mut self) -> ExpressionTree {
        ExpressionTree {
            loc: self.current_loc(),
            expression: {
                let primary_expression = match self.peek_token() {
                    Token::Identifier(value) => {
                        self.next_token();
                        Expression::Identifier(value)
                    }
                    Token::UnsignedDecimalConstant(value) => {
                        self.next_token();
                        Expression::Constant(value)
                    }
                    Token::LParen => {
                        self.expect_token(Token::LParen);
                        let expr = self.parse_expression();
                        self.expect_token(Token::RParen);
                        expr.expression
                    } // TODO: don't duplciate ExpressionTree here
                    _ => self.unexpected_token(),
                };
                primary_expression
            },
        }
    }

    fn token_is_operator_of_at_least_precedence(token: &Token, precedence: usize) -> bool {
        match token {
            Token::Plus | Token::Minus | Token::Star | Token::FSlash => {
                OperatorPrecedences::of_token(&token) >= precedence
            }
            _ => false,
        }
    }

    pub fn parse_expression_min_precedence(
        &mut self,
        lhs: ExpressionTree,
        min_precedence: usize,
    ) -> ExpressionTree {
        let mut expr = lhs;
        let start_loc = self.current_loc();
        while Self::token_is_operator_of_at_least_precedence(&self.peek_token(), min_precedence) {
            let operation = self.next_token();
            let mut rhs = self.parse_primary_expression();

            while Self::token_is_operator_of_at_least_precedence(
                &self.peek_token(),
                OperatorPrecedences::of_token(&operation),
            ) {
                rhs = self.parse_expression_min_precedence(
                    rhs,
                    OperatorPrecedences::of_token(&operation),
                );
            }

            let operands = ArithmeticDualOperands {
                e1: Box::new(expr),
                e2: Box::new(rhs),
            };
            expr = ExpressionTree {
                loc: start_loc,
                expression: Expression::Arithmetic(match operation {
                    Token::Plus => ArithmeticOperation::Add(operands),
                    Token::Minus => ArithmeticOperation::Subtract(operands),
                    Token::Star => ArithmeticOperation::Multiply(operands),
                    Token::FSlash => ArithmeticOperation::Divide(operands),
                    _ => self.unexpected_token(),
                }),
            };
        }
        expr
    }

    fn parse_expression(&mut self) -> ExpressionTree {
        let lhs = self.parse_primary_expression();
        match self.peek_token() {
            Token::Plus | Token::Minus | Token::Star | Token::FSlash => {
                self.parse_expression_min_precedence(lhs, 0)
            }
            _ => lhs,
        }
    }

    fn parse_function_prototype(&mut self) -> FunctionDeclarationTree {
        let start_loc = self.current_loc();
        // start with fun
        self.expect_token(Token::Fun);
        FunctionDeclarationTree {
            // grab start location and name
            loc: start_loc,
            name: self.parse_identifier(),
            arguments: {
                self.expect_token(Token::LParen);
                let mut arguments = Vec::<VariableDeclarationTree>::new();
                loop {
                    match self.peek_token() {
                        // argument declaration
                        Token::U8 | Token::U16 | Token::U32 | Token::U64 => {
                            arguments.push(self.parse_variable_declaration());
                            match self.peek_token() {
                                Token::Comma => self.next_token(), // expect another argument declaration after comma
                                _ => break,                        // loop again for anything else
                            };
                        }
                        Token::RParen => break, // done on rparen
                        _ => self.unexpected_token(),
                    }
                }
                // consume closing paren
                self.expect_token(Token::RParen);
                arguments
            },
            return_type: match self.peek_token() {
                Token::Arrow => {
                    self.next_token();
                    Some(self.parse_typename())
                }
                _ => None,
            },
        }
    }

    fn parse_variable_declaration(&mut self) -> VariableDeclarationTree {
        VariableDeclarationTree {
            loc: self.current_loc(),
            typename: self.parse_typename(),
            name: self.parse_identifier(),
        }
    }

    fn parse_typename(&mut self) -> TypenameTree {
        TypenameTree {
            loc: self.current_loc(),
            name: match self.peek_token() {
                Token::U8 => {
                    self.next_token();
                    String::from("u8")
                }
                Token::U16 => {
                    self.next_token();
                    String::from("u16")
                }
                Token::U32 => {
                    self.next_token();
                    String::from("u32")
                }
                Token::U64 => {
                    self.next_token();
                    String::from("u64")
                }
                _ => self.unexpected_token(),
            },
        }
    }

    fn parse_identifier(&mut self) -> String {
        match self.expect_token(Token::Identifier(String::from(""))) {
            Token::Identifier(value) => value,
            _ => self.unexpected_token::<String>(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::Lexer;
    use crate::Parser;
    use std::str::Chars;

    fn parser_from_string(input: &str) -> Parser<Chars<'_>> {
        Parser::new(Lexer::new(input.chars()))
    }

    fn parse_and_print_expression(input: &str) -> String {
        let mut parser = parser_from_string(input);
        let expr_string = parser.parse_expression().to_string();
        parser.expect_token(super::Token::Eof);
        expr_string
    }

    #[test]
    fn parse_basic_expression() {
        assert_eq!(
            parse_and_print_expression("123 + 456 + 789"),
            "(123 + (456 + 789))"
        );
    }

    #[test]
    fn parse_addition_and_multiplication() {
        assert_eq!(
            parse_and_print_expression("123 + 456 * 789"),
            "(123 + (456 * 789))"
        );
    }

    #[test]
    fn parse_parentheses_override_precedence() {
        assert_eq!(
            parse_and_print_expression("(123 + 456) * 789"),
            "((123 + 456) * 789)"
        );
    }

    #[test]
    fn parse_mixed_operations() {
        assert_eq!(
            parse_and_print_expression("1 + 2 * 3 - 4 / 5"),
            "(1 + ((2 * 3) - (4 / 5)))"
        );
    }

    #[test]
    fn parse_nested_parentheses() {
        assert_eq!(
            parse_and_print_expression("((1 + 2) * (3 - 4)) / 5"),
            "(((1 + 2) * (3 - 4)) / 5)"
        );
    }

    #[test]
    fn parse_single_number() {
        assert_eq!(parse_and_print_expression("42"), "42");
    }

    #[test]
    fn parse_single_number_parenthesized() {
        assert_eq!(parse_and_print_expression("(42)"), "42");
    }

    #[test]
    fn parse_multiple_additions() {
        assert_eq!(parse_and_print_expression("1 + 2 + 3"), "(1 + (2 + 3))");
    }

    #[test]
    fn parse_complex_expression() {
        assert_eq!(
            parse_and_print_expression("3 + 4 * 2 / (1 - 5)"),
            "(3 + (4 * (2 / (1 - 5))))"
        );
    }
}
