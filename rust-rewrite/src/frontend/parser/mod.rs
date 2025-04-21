mod tests;

use crate::midend::{ir, types::Type};

use super::{
    ast::*,
    lexer::{token::Token, *},
    sourceloc::SourceLoc,
};

pub struct Parser<'a> {
    lexer: Lexer<'a>,
}

impl ir::BinaryOperations {
    pub fn get_precedence(&self) -> usize {
        match self {
            Self::Add(_) => 1,
            Self::Subtract(_) => 1,
            Self::Multiply(_) => 2,
            Self::Divide(_) => 2,
            Self::LThan(_) => 3,
            Self::GThan(_) => 3,
            Self::LThanE(_) => 3,
            Self::GThanE(_) => 3,
            Self::Equals(_) => 4,
            Self::NotEquals(_) => 4,
        }
    }

    pub fn precedence_of_token(token: &Token) -> usize {
        match token {
            Token::Plus => 1,
            Token::Minus => 1,
            Token::Star => 2,
            Token::FSlash => 2,
            Token::LThan => 3,
            Token::GThan => 3,
            Token::LThanE => 3,
            Token::GThanE => 3,
            Token::Equals => 4,
            Token::NotEquals => 4,
            _ => {
                panic!(
                    "Invalid token {} passed to BinaryOperations::precedence_of_token",
                    token
                );
            }
        }
    }
}

impl<'a> Parser<'a> {
    pub fn new(lexer: Lexer<'a>) -> Self {
        Parser { lexer: lexer }
    }

    fn peek_token(&self) -> Token {
        return self.lexer.peek();
    }

    fn next_token(&mut self) -> Token {
        return self.lexer.next();
    }

    fn expect_token(&mut self, _t: Token) -> Token {
        if matches!(self.peek_token(), _t) {
            self.next_token()
        } else {
            panic!(
                "Expected token {} at {}, got token {} instead!",
                _t,
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

    fn parse_translation_unit(&mut self) -> TranslationUnitTree {
        TranslationUnitTree {
            loc: self.current_loc(),
            contents: match self.peek_token() {
                Token::Fun => self.parse_function_declaration_or_definition(),
                Token::Struct => self.parse_struct_definition(),
                _ => self.unexpected_token::<TranslationUnit>(),
            },
        }
    }

    fn parse_function_declaration_or_definition(&mut self) -> TranslationUnit {
        let function_declaration = self.parse_function_prototype();
        match self.peek_token() {
            Token::LCurly => TranslationUnit::FunctionDefinition(FunctionDefinitionTree {
                prototype: function_declaration,
                body: self.parse_compound_statement(),
            }),
            _ => TranslationUnit::FunctionDeclaration(function_declaration),
        }
    }

    fn parse_struct_definition(&mut self) -> TranslationUnit {
        let loc = self.current_loc();

        self.expect_token(Token::Struct);
        let struct_name = self.parse_identifier();
        self.expect_token(Token::LCurly);

        let mut struct_fields = Vec::new();

        loop {
            match self.peek_token() {
                Token::Identifier(identifier) => {
                    self.next_token();
                    struct_fields.push(self.parse_variable_declaration(identifier));

                    if matches!(self.peek_token(), Token::Comma) {
                        self.next_token();
                    }
                }
                Token::RCurly => {
                    self.next_token();
                    break;
                }
                _ => {
                    self.unexpected_token::<()>();
                }
            }
        }

        TranslationUnit::StructDefinition(StructDefinitionTree {
            name: struct_name,
            fields: struct_fields,
        })
    }

    fn parse_compound_statement(&mut self) -> CompoundStatementTree {
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

    fn parse_statement(&mut self) -> StatementTree {
        let statement_tree = StatementTree {
            loc: self.current_loc(),
            statement: match self.peek_token() {
                Token::Identifier(identifier) => {
                    self.next_token();
                    self.parse_identifier_statement(identifier)
                }
                Token::If => Statement::IfStatement(self.parse_if_statement()),
                Token::While => Statement::WhileLoop(self.parse_while_loop()),
                _ => self.unexpected_token(),
            },
        };
        statement_tree
    }

    fn parse_identifier_statement(&mut self, identifier: String) -> Statement {
        let statement = match self.peek_token() {
            Token::Colon => {
                Statement::VariableDeclaration(self.parse_variable_declaration(identifier))
            }
            Token::Assign => Statement::Assignment(self.parse_assignment(identifier)),

            _ => self.unexpected_token(),
        };
        self.expect_token(Token::Semicolon);
        statement
    }

    fn parse_if_statement(&mut self) -> IfStatementTree {
        let start_loc = self.current_loc();
        self.expect_token(Token::If);

        self.expect_token(Token::LParen);
        let condition: ExpressionTree = self.parse_expression();
        self.expect_token(Token::RParen);

        let true_block = self.parse_compound_statement();
        let false_block = match self.peek_token() {
            Token::Else => {
                self.next_token();
                Some(self.parse_compound_statement())
            }
            _ => None,
        };

        IfStatementTree {
            loc: start_loc,
            condition,
            true_block,
            false_block,
        }
    }

    fn parse_while_loop(&mut self) -> WhileLoopTree {
        let start_loc = self.current_loc();
        self.expect_token(Token::While);

        self.expect_token(Token::LParen);
        let condition = self.parse_expression();
        self.expect_token(Token::RParen);

        let body = self.parse_compound_statement();

        WhileLoopTree {
            loc: start_loc,
            condition,
            body,
        }
    }

    fn parse_assignment(&mut self, identifier: String) -> AssignmentTree {
        let start_loc = self.current_loc();
        let lhs = identifier;
        self.expect_token(Token::Assign);
        AssignmentTree {
            loc: start_loc,
            assignee: self.parse_expression(),
            value: self.parse_expression(),
        }
    }

    fn parse_primary_expression(&mut self) -> ExpressionTree {
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
                        Expression::UnsignedDecimalConstant(value)
                    }
                    Token::LParen => {
                        self.next_token();
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
            Token::Plus
            | Token::Minus
            | Token::Star
            | Token::FSlash
            | Token::LThan
            | Token::GThan
            | Token::LThanE
            | Token::GThanE
            | Token::Equals
            | Token::NotEquals => ir::BinaryOperations::precedence_of_token(&token) >= precedence,
            _ => false,
        }
    }

    fn parse_expression_min_precedence(
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
                ir::BinaryOperations::precedence_of_token(&operation),
            ) {
                rhs = self.parse_expression_min_precedence(
                    rhs,
                    ir::BinaryOperations::precedence_of_token(&operation),
                );
            }

            let operands = ArithmeticDualOperands {
                e1: Box::new(expr),
                e2: Box::new(rhs),
            };
            expr = ExpressionTree {
                loc: start_loc,
                expression: match operation {
                    Token::Plus => Expression::Arithmetic(ArithmeticOperationTree::Add(operands)),
                    Token::Minus => {
                        Expression::Arithmetic(ArithmeticOperationTree::Subtract(operands))
                    }
                    Token::Star => {
                        Expression::Arithmetic(ArithmeticOperationTree::Multiply(operands))
                    }
                    Token::FSlash => {
                        Expression::Arithmetic(ArithmeticOperationTree::Divide(operands))
                    }
                    Token::LThan => {
                        Expression::Comparison(ComparisonOperationTree::LThan(operands))
                    }
                    Token::GThan => {
                        Expression::Comparison(ComparisonOperationTree::GThan(operands))
                    }
                    Token::LThanE => {
                        Expression::Comparison(ComparisonOperationTree::LThanE(operands))
                    }
                    Token::GThanE => {
                        Expression::Comparison(ComparisonOperationTree::GThanE(operands))
                    }
                    Token::Equals => {
                        Expression::Comparison(ComparisonOperationTree::Equals(operands))
                    }
                    Token::NotEquals => {
                        Expression::Comparison(ComparisonOperationTree::NotEquals(operands))
                    }
                    _ => self.unexpected_token(),
                },
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
            Token::GThan
            | Token::GThanE
            | Token::LThan
            | Token::LThanE
            | Token::Equals
            | Token::NotEquals => self.parse_expression_min_precedence(lhs, 0),
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
                        Token::Identifier(identifier) => {
                            self.next_token();
                            arguments.push(self.parse_variable_declaration(identifier));
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

    // TODO: pass loc of string to get true start loc of declaration
    fn parse_variable_declaration(&mut self, name: String) -> VariableDeclarationTree {
        VariableDeclarationTree {
            loc: self.current_loc(),
            name,
            typename: {
                self.expect_token(Token::Colon);
                self.parse_typename()
            },
        }
    }

    fn parse_typename(&mut self) -> TypenameTree {
        TypenameTree {
            loc: self.current_loc(),
            type_: match self.peek_token() {
                Token::U8 => {
                    self.next_token();
                    Type::U8
                }
                Token::U16 => {
                    self.next_token();
                    Type::U16
                }
                Token::U32 => {
                    self.next_token();
                    Type::U32
                }
                Token::U64 => {
                    self.next_token();
                    Type::U64
                }
                Token::I8 => {
                    self.next_token();
                    Type::I8
                }
                Token::I16 => {
                    self.next_token();
                    Type::I16
                }
                Token::I32 => {
                    self.next_token();
                    Type::I32
                }
                Token::I64 => {
                    self.next_token();
                    Type::I64
                }
                Token::Identifier(name) => {
                    self.next_token();
                    Type::UDT(name)
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
