mod tests;

use crate::midend::{ir, types::Type};

use super::{
    ast::*,
    lexer::{token::Token, *},
    sourceloc::SourceLoc,
};

pub struct Parser<'a> {
    lexer: Lexer<'a>,
    #[cfg(feature = "loud_parsing")]
    parsing_stack: Vec<String>,
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
            #[cfg(feature = "loud_parsing")]
            parsing_stack: Vec::new(),
        }
    }

    fn peek_token(&mut self) -> Token {
        let peeked = self.lexer.peek();
        // #[cfg(feature = "loud_parsing")]
        // println!("Parser::peek_token() -> {}", peeked);
        return peeked;
    }

    fn next_token(&mut self) -> Token {
        let next = self.lexer.next();
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
        panic!(
            "Unexpected token {} at {}",
            self.peek_token(),
            self.current_loc()
        );
    }
}

#[cfg(feature = "loud_parsing")]
impl<'a> Parser<'a> {
    fn start_parsing(&mut self, what_parsing: &str) {
        for i in 0..self.parsing_stack.len() {
            print!("\t");
        }
        println!("Start parsing {}", what_parsing);
        self.parsing_stack.push(String::from(what_parsing));
    }

    fn finish_parsing<T>(&mut self, parsed: &T)
    where
        T: std::fmt::Display,
    {
        let parsed_description = self
            .parsing_stack
            .pop()
            .expect("Mismatched loud parsing tracking");
        for i in 0..self.parsing_stack.len() {
            print!("\t");
        }
        println!("Done parsing {}: {}", parsed_description, parsed);
    }
}

impl<'a> Parser<'a> {
    pub fn parse(&mut self) -> Vec<TranslationUnitTree> {
        let mut translation_units = Vec::new();
        while self.lexer.peek() != Token::Eof {
            translation_units.push(self.parse_translation_unit());
        }
        translation_units
    }

    fn parse_translation_unit(&mut self) -> TranslationUnitTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("translation unit");

        let translation_unit = TranslationUnitTree {
            loc: self.current_loc(),
            contents: match self.peek_token() {
                Token::Fun => self.parse_function_declaration_or_definition(),
                Token::Struct => self.parse_struct_definition(),
                _ => self.unexpected_token::<TranslationUnit>(),
            },
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&translation_unit);

        translation_unit
    }

    fn parse_function_declaration_or_definition(&mut self) -> TranslationUnit {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("function declaration/definition");

        let function_declaration = self.parse_function_prototype();
        let function_declaration_or_definition = match self.peek_token() {
            Token::LCurly => TranslationUnit::FunctionDefinition(FunctionDefinitionTree {
                prototype: function_declaration,
                body: self.parse_block_expression(),
            }),
            _ => TranslationUnit::FunctionDeclaration(function_declaration),
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&function_declaration_or_definition);

        function_declaration_or_definition
    }

    fn parse_struct_definition(&mut self) -> TranslationUnit {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("struct definition");

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

        let struct_definition = TranslationUnit::StructDefinition(StructDefinitionTree {
            name: struct_name,
            fields: struct_fields,
        });

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&struct_definition);

        struct_definition
    }

    fn parse_block_expression(&mut self) -> CompoundExpressionTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("compound statement");

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

        let compound_statement = CompoundExpressionTree {
            loc: start_loc,
            statements: statements,
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&compound_statement);

        compound_statement
    }

    fn parse_statement(&mut self) -> StatementTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("statement");

        let statement = StatementTree {
            loc: self.current_loc(),
            statement: match self.peek_token() {
                Token::If | Token::While | Token::LCurly | Token::Identifier(_) => {
                    Statement::Expression(self.parse_expression())
                }
                _ => self.unexpected_token(),
            },
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&statement);

        statement
    }

    fn parse_identifier_statement(&mut self, identifier: String) -> Statement {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("identifier statement");

        let identifier_statement = match self.peek_token() {
            Token::Colon => {
                Statement::VariableDeclaration(self.parse_variable_declaration(identifier))
            }
            Token::Assign => Statement::Assignment(self.parse_assignment(identifier)),

            _ => self.unexpected_token(),
        };
        self.expect_token(Token::Semicolon);

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&identifier_statement);

        identifier_statement
    }

    fn parse_if_expression(&mut self) -> ExpressionTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("if statement");

        let start_loc = self.current_loc();
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

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&if_expression);

        if_expression
    }

    fn parse_while_expression(&mut self) -> WhileLoopTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("while loop");

        let start_loc = self.current_loc();
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

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&while_loop);

        while_loop
    }

    fn parse_assignment(&mut self, identifier: String) -> AssignmentTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("assignment");

        let start_loc = self.current_loc();
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

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&assignment);

        assignment
    }

    fn parse_primary_expression(&mut self) -> ExpressionTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("primary expression");

        let primary_expression = ExpressionTree {
            loc: self.current_loc(),
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

        #[cfg(feature = "loud_parsing")]
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

    fn parse_expression_min_precedence(
        &mut self,
        lhs: ExpressionTree,
        min_precedence: usize,
    ) -> ExpressionTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing(&format!("expression (min precedence: {})", min_precedence));

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

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&expr);

        expr
    }

    fn parse_expression(&mut self) -> ExpressionTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("expression");

        let expr = match self.lexer.peek() {
            Token::If => self.parse_if_expression(),
            // Token::While => self.parse_while_expression(),
            Token::Identifier(ident) => self.parse_identifier_expression(),
            _ => self.unexpected_token(),
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&expr);

        expr
    }

    fn parse_identifier_expression(&mut self) -> ExpressionTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("identifier expression");

        let primary_expression = ExpressionTree {
            loc: self.current_loc(),
            expression: Expression::Identifier(self.parse_identifier()),
        };
        let expr = match self.peek_token() {
            Token::Plus | Token::Minus | Token::Star | Token::FSlash => {
                self.parse_expression_min_precedence(primary_expression, 0)
            }
            Token::GThan
            | Token::GThanE
            | Token::LThan
            | Token::LThanE
            | Token::Equals
            | Token::NotEquals => self.parse_expression_min_precedence(primary_expression, 0),
            _ => primary_expression,
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&expr);

        expr
    }

    fn parse_function_prototype(&mut self) -> FunctionDeclarationTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("function prototype");

        let start_loc = self.current_loc();
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

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&prototype);

        prototype
    }

    // TODO: pass loc of string to get true start loc of declaration
    fn parse_variable_declaration(&mut self, name: String) -> VariableDeclarationTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("function prototype");

        let declaration = VariableDeclarationTree {
            loc: self.current_loc(),
            name,
            typename: {
                self.expect_token(Token::Colon);
                self.parse_typename()
            },
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&declaration);

        declaration
    }

    fn parse_typename(&mut self) -> TypenameTree {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("typename");

        let typename = TypenameTree {
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
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&typename);

        typename
    }

    fn parse_identifier(&mut self) -> String {
        #[cfg(feature = "loud_parsing")]
        self.start_parsing("identifier");

        let identifier = match self.expect_token(Token::Identifier(String::from(""))) {
            Token::Identifier(value) => value,
            _ => self.unexpected_token::<String>(),
        };

        #[cfg(feature = "loud_parsing")]
        self.finish_parsing(&identifier);

        identifier
    }
}
