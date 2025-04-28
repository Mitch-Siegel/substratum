#[cfg(test)]
mod tests;

use std::collections::VecDeque;

use crate::midend::{ir, types::Type};

use super::{
    ast::*,
    lexer::{token::Token, *},
    sourceloc::SourceLoc,
};

pub struct Parser<'a> {
    lexer: Lexer<'a>,
    upcoming_tokens: VecDeque<Token>,
    parsing_stack: Vec<(SourceLoc, String)>,
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
        Parser {
            lexer: lexer,
            upcoming_tokens: VecDeque::new(),
            parsing_stack: Vec::new(),
        }
    }

    fn ensure_n_tokens_in_lookahead(&mut self, n: usize) {
        while self.upcoming_tokens.len() <= n && self.lexer.peek() != Token::Eof {
            self.upcoming_tokens.push_back(self.lexer.next());
        }
    }

    // return the next token from the input stream without advancing
    // utilizes lookahead_token
    fn peek_token(&mut self) -> Token {
        let peeked = self.lookahead_token(0);
        // #[cfg(feature = "loud_parsing")]
        // println!("Parser::peek_token() -> {}", peeked);
        return peeked;
    }

    // returns the lookahead_by-th token from the input stream withing advancing, or EOF if that many tokens are not available
    fn lookahead_token(&mut self, lookahead_by: usize) -> Token {
        self.ensure_n_tokens_in_lookahead(lookahead_by);

        self.upcoming_tokens
            .get(lookahead_by)
            .cloned()
            .unwrap_or(Token::Eof)
    }

    fn next_token(&mut self) -> Token {
        self.ensure_n_tokens_in_lookahead(1);
        let next = self.upcoming_tokens.pop_front().unwrap_or(Token::Eof);
        #[cfg(feature = "loud_parsing")]
        println!("Parser::next_token() -> {}", next);
        next
    }

    fn expect_token(&mut self, _t: Token) -> Token {
        #[cfg(feature = "loud_parsing")]
        println!("Parser::expect_token({})", _t);

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

    fn unexpected_token<T>(&mut self) -> T {
        let (current_parse_loc, current_parse_str) = self
            .parsing_stack
            .last()
            .unwrap_or(&(SourceLoc::none(), String::from("UNKNOWN")))
            .to_owned();
        panic!(
            "Unexpected token {} at {} while parsing {} (started at {})",
            self.peek_token(),
            self.current_loc(),
            current_parse_str,
            current_parse_loc
        );
    }

    fn start_parsing(&mut self, what_parsing: &str) -> SourceLoc {
        for i in 0..self.parsing_stack.len() {
            print!("\t");
        }
        #[cfg(feature = "loud_parsing")]
        println!("Start parsing {}", what_parsing);

        self.parsing_stack
            .push((self.current_loc(), String::from(what_parsing)));
        self.current_loc()
    }

    fn finish_parsing<T>(&mut self, parsed: &T)
    where
        T: std::fmt::Display,
    {
        let (parse_start, parsed_description) = self
            .parsing_stack
            .pop()
            .expect("Mismatched loud parsing tracking");
        for i in 0..self.parsing_stack.len() {
            print!("\t");
        }

        #[cfg(feature = "loud_parsing")]
        println!(
            "Done parsing {} ({}-{}): {}",
            parsed_description,
            parse_start,
            self.current_loc(),
            parsed
        );
    }
}

impl<'a> Parser<'a> {
    pub fn parse(&mut self) -> Vec<TranslationUnitTree> {
        let mut translation_units = Vec::new();
        while self.peek_token() != Token::Eof {
            translation_units.push(self.parse_translation_unit());
        }
        translation_units
    }

    fn parse_translation_unit(&mut self) -> TranslationUnitTree {
        let start_loc = self.start_parsing("translation unit");

        let translation_unit = TranslationUnitTree {
            loc: start_loc,
            contents: match self.peek_token() {
                Token::Fun => self.parse_function_declaration_or_definition(),
                Token::Struct => self.parse_struct_definition(),
                _ => self.unexpected_token::<TranslationUnit>(),
            },
        };

        self.finish_parsing(&translation_unit);

        translation_unit
    }

    fn parse_function_declaration_or_definition(&mut self) -> TranslationUnit {
        let _start_loc = self.start_parsing("function declaration/definition");

        let function_declaration = self.parse_function_prototype();
        let function_declaration_or_definition = match self.peek_token() {
            Token::LCurly => TranslationUnit::FunctionDefinition(FunctionDefinitionTree {
                prototype: function_declaration,
                body: self.parse_block_expression(),
            }),
            _ => TranslationUnit::FunctionDeclaration(function_declaration),
        };

        self.finish_parsing(&function_declaration_or_definition);

        function_declaration_or_definition
    }

    fn parse_struct_definition(&mut self) -> TranslationUnit {
        let start_loc = self.start_parsing("struct definition");

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

        let struct_definition = TranslationUnit::StructDefinition(StructDefinitionTree {
            loc: start_loc,
            name: struct_name,
            fields: struct_fields,
        });

        self.finish_parsing(&struct_definition);

        struct_definition
    }

    fn parse_block_expression(&mut self) -> CompoundExpressionTree {
        let start_loc = self.start_parsing("compound statement");

        self.expect_token(Token::LCurly);
        let mut statements: Vec<StatementTree> = Vec::new();
        loop {
            match self.peek_token() {
                Token::RCurly => break,
                _ => statements.push(self.parse_statement()),
            }
        }
        self.expect_token(Token::RCurly);

        let compound_statement = CompoundExpressionTree {
            loc: start_loc,
            statements: statements,
        };

        self.finish_parsing(&compound_statement);

        compound_statement
    }

    fn parse_statement(&mut self) -> StatementTree {
        let start_loc = self.start_parsing("statement");

        let statement = StatementTree {
            loc: self.current_loc(),
            statement: match self.peek_token() {
                Token::If | Token::While | Token::LCurly | Token::Identifier(_) => {
                    let expression = self.parse_expression();
                    match self.peek_token() {
                        Token::Assign => {
                            self.expect_token(Token::Assign);
                            let rhs = self.parse_expression();
                            Statement::Assignment(AssignmentTree {
                                loc: start_loc,
                                assignee: expression,
                                value: rhs,
                            })
                        }
                        _ => Statement::Expression(expression),
                    }
                }
                _ => self.unexpected_token(),
            },
        };

        if matches!(self.peek_token(), Token::Semicolon) {
            self.expect_token(Token::Semicolon);
        }

        self.finish_parsing(&statement);

        statement
    }

    fn parse_if_expression(&mut self) -> ExpressionTree {
        let start_loc = self.start_parsing("if statement");

        self.expect_token(Token::If);

        self.expect_token(Token::LParen);
        let condition: ExpressionTree = self.parse_expression();
        self.expect_token(Token::RParen);

        let true_block = self.parse_block_expression();
        let false_block = match self.peek_token() {
            Token::Else => {
                self.next_token();
                Some(self.parse_block_expression())
            }
            _ => None,
        };

        let if_expression = ExpressionTree {
            loc: start_loc,
            expression: Expression::If(Box::new(IfExpressionTree {
                loc: start_loc,
                condition,
                true_block,
                false_block,
            })),
        };

        self.finish_parsing(&if_expression);

        if_expression
    }

    fn parse_while_expression(&mut self) -> WhileLoopTree {
        let start_loc = self.start_parsing("while loop");

        self.expect_token(Token::While);

        self.expect_token(Token::LParen);
        let condition = self.parse_expression();
        self.expect_token(Token::RParen);

        let body = self.parse_block_expression();

        let while_loop = WhileLoopTree {
            loc: start_loc,
            condition,
            body,
        };

        self.finish_parsing(&while_loop);

        while_loop
    }

    fn parse_assignment(&mut self, identifier: String) -> AssignmentTree {
        let start_loc = self.start_parsing("assignment");

        let lhs = identifier;
        self.expect_token(Token::Assign);
        let assignment = AssignmentTree {
            loc: start_loc,
            assignee: ExpressionTree {
                loc: start_loc,
                expression: Expression::Identifier(lhs),
            },
            value: self.parse_expression(),
        };

        self.finish_parsing(&assignment);

        assignment
    }

    fn parse_primary_expression(&mut self) -> ExpressionTree {
        let start_loc = self.start_parsing("primary expression");

        let primary_expression = ExpressionTree {
            loc: start_loc,
            expression: {
                match self.peek_token() {
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
                }
            },
        };

        self.finish_parsing(&primary_expression);

        primary_expression
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

    fn parse_binary_expression_min_precedence(
        &mut self,
        lhs: ExpressionTree,
        min_precedence: usize,
    ) -> ExpressionTree {
        self.start_parsing(&format!("expression (min precedence: {})", min_precedence));
        let start_loc = lhs.loc;

        let mut expr = lhs;
        while Self::token_is_operator_of_at_least_precedence(&self.peek_token(), min_precedence) {
            let operation = self.next_token();
            let mut rhs = self.parse_primary_expression();

            while Self::token_is_operator_of_at_least_precedence(
                &self.peek_token(),
                ir::BinaryOperations::precedence_of_token(&operation),
            ) {
                rhs = self.parse_binary_expression_min_precedence(
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
                    Token::Plus => Expression::Arithmetic(ArithmeticExpressionTree::Add(operands)),
                    Token::Minus => {
                        Expression::Arithmetic(ArithmeticExpressionTree::Subtract(operands))
                    }
                    Token::Star => {
                        Expression::Arithmetic(ArithmeticExpressionTree::Multiply(operands))
                    }
                    Token::FSlash => {
                        Expression::Arithmetic(ArithmeticExpressionTree::Divide(operands))
                    }
                    Token::LThan => {
                        Expression::Comparison(ComparisonExpressionTree::LThan(operands))
                    }
                    Token::GThan => {
                        Expression::Comparison(ComparisonExpressionTree::GThan(operands))
                    }
                    Token::LThanE => {
                        Expression::Comparison(ComparisonExpressionTree::LThanE(operands))
                    }
                    Token::GThanE => {
                        Expression::Comparison(ComparisonExpressionTree::GThanE(operands))
                    }
                    Token::Equals => {
                        Expression::Comparison(ComparisonExpressionTree::Equals(operands))
                    }
                    Token::NotEquals => {
                        Expression::Comparison(ComparisonExpressionTree::NotEquals(operands))
                    }
                    _ => self.unexpected_token(),
                },
            };
        }

        self.finish_parsing(&expr);

        expr
    }

    fn parse_expression(&mut self) -> ExpressionTree {
        let _start_loc = self.start_parsing("expression");

        let mut expr = match self.peek_token() {
            Token::If => self.parse_if_expression(),
            // Token::While => self.parse_while_expression(),
            Token::Identifier(ident) => self.parse_identifier_expression(),
            Token::UnsignedDecimalConstant(_) => self.parse_literal_expression(),
            Token::LParen => self.parse_parenthesized_expression(),
            _ => self.unexpected_token(),
        };

        let peeked = self.peek_token();
        match peeked {
            Token::Plus
            | Token::Minus
            | Token::Star
            | Token::FSlash
            | Token::LThan
            | Token::GThan
            | Token::LThanE
            | Token::GThanE
            | Token::Equals
            | Token::NotEquals => {
                let lhs = expr;
                expr = self.parse_binary_expression_min_precedence(lhs, 0)
            }

            _ => {
                #[cfg(feature = "loud_parsing")]
                println!("Peeked {} after lhs {} of expression", peeked, expr);
            }
        }

        self.finish_parsing(&expr);

        expr
    }

    fn parse_identifier_expression(&mut self) -> ExpressionTree {
        let _start_loc = self.start_parsing("identifier expression");

        let expr = ExpressionTree {
            loc: self.current_loc(),
            expression: Expression::Identifier(self.parse_identifier()),
        };

        self.finish_parsing(&expr);

        expr
    }

    fn parse_literal_expression(&mut self) -> ExpressionTree {
        let _start_loc = self.start_parsing("literal expression");

        let expr = ExpressionTree {
            loc: self.current_loc(),
            expression: match self.peek_token() {
                Token::UnsignedDecimalConstant(value) => {
                    self.next_token();
                    Expression::UnsignedDecimalConstant(value)
                }
                _ => self.unexpected_token(),
            },
        };

        self.finish_parsing(&expr);

        expr
    }

    fn parse_parenthesized_expression(&mut self) -> ExpressionTree {
        let _start_loc = self.start_parsing("parenthesized expression");

        self.expect_token(Token::LParen);
        let parenthesized_expr = self.parse_expression();
        self.expect_token(Token::RParen);

        self.finish_parsing(&parenthesized_expr);
        parenthesized_expr
    }

    fn parse_function_prototype(&mut self) -> FunctionDeclarationTree {
        let start_loc = self.start_parsing("function prototype");

        // start with fun
        self.expect_token(Token::Fun);
        let prototype = FunctionDeclarationTree {
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
        };

        self.finish_parsing(&prototype);

        prototype
    }

    // TODO: pass loc of string to get true start loc of declaration
    fn parse_variable_declaration(&mut self, name: String) -> VariableDeclarationTree {
        let start_loc = self.start_parsing("function prototype");

        let declaration = VariableDeclarationTree {
            loc: start_loc,
            name,
            typename: {
                self.expect_token(Token::Colon);
                self.parse_typename()
            },
        };

        self.finish_parsing(&declaration);

        declaration
    }

    fn parse_typename(&mut self) -> TypenameTree {
        let start_loc = self.start_parsing("typename");

        let typename = TypenameTree {
            loc: start_loc,
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
        };

        self.finish_parsing(&typename);

        typename
    }

    fn parse_identifier(&mut self) -> String {
        let _start_loc = self.start_parsing("identifier");

        let identifier = match self.expect_token(Token::Identifier(String::from(""))) {
            Token::Identifier(value) => value,
            _ => self.unexpected_token::<String>(),
        };

        self.finish_parsing(&identifier);

        identifier
    }
}
