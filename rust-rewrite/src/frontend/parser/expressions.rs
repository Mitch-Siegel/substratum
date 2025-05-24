use crate::{
    frontend::{ast::*, lexer::token::Token},
    midend::ir,
};

use super::{ParseError, Parser};

// parsing functions which yield an ExpressionTree
impl<'a> Parser<'a> {
    pub fn parse_block_expression(&mut self) -> Result<CompoundExpressionTree, ParseError> {
        let start_loc = self.start_parsing("compound statement")?;

        self.expect_token(Token::LCurly)?;
        let mut statements: Vec<StatementTree> = Vec::new();
        loop {
            match self.peek_token()? {
                Token::RCurly => break,
                _ => statements.push(self.parse_statement()?),
            }
        }
        self.expect_token(Token::RCurly)?;

        let compound_statement = CompoundExpressionTree {
            loc: start_loc,
            statements: statements,
        };

        self.finish_parsing(&compound_statement)?;

        Ok(compound_statement)
    }

    pub fn expression_starters() -> [Token; 5] {
        [
            Token::If,
            Token::While,
            Token::Identifier("".into()),
            Token::UnsignedDecimalConstant(0),
            Token::LParen,
        ]
    }

    pub fn token_starts_expression(t: Token) -> bool {
        match t {
            Token::If
            | Token::While
            | Token::Identifier(_)
            | Token::UnsignedDecimalConstant(_)
            | Token::LParen => true,
            _ => false,
        }
    }

    pub fn parse_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let _start_loc = self.start_parsing("expression")?;

        assert!(
            Self::token_starts_expression(self.peek_token()?),
            "{} does not start an expression",
            self.peek_token()?
        ); // sanity-check this method call to self-validate

        let mut expr = match self.peek_token()? {
            Token::If => self.parse_if_expression()?,
            Token::While => self.parse_while_expression()?,
            Token::Identifier(_) => self.parse_identifier_expression()?,
            Token::UnsignedDecimalConstant(_) => self.parse_literal_expression()?,
            Token::LParen => self.parse_parenthesized_expression()?,
            _ => self.unexpected_token(&[
                Token::If,
                Token::While,
                Token::Identifier("".into()),
                Token::UnsignedDecimalConstant(0),
                Token::LParen,
            ])?,
        };

        match self.peek_token()? {
            Token::Dot => {
                if matches!(self.lookahead_token(2)?, Token::LParen) {
                    expr = self.parse_method_call_expression(expr)?;
                } else {
                    expr = self.parse_field_expression(expr)?;
                }
            }
            _ => {}
        }

        let peeked = self.peek_token()?;
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
                expr = self.parse_binary_expression_min_precedence(lhs, 0)?
            }

            _ => {
                #[cfg(feature = "loud_parsing")]
                println!("Peeked {} after lhs {} of expression", peeked, expr);
            }
        }

        match self.peek_token()? {
            Token::Assign => {
                expr = self.parse_assignment_expression(expr)?;
            }
            _ => {}
        }

        self.finish_parsing(&expr)?;

        Ok(expr)
    }

    pub fn parse_parenthesized_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let _start_loc = self.start_parsing("parenthesized expression")?;

        self.expect_token(Token::LParen)?;
        let parenthesized_expr = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        self.finish_parsing(&parenthesized_expr)?;
        Ok(parenthesized_expr)
    }

    pub fn parse_if_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let start_loc = self.start_parsing("if statement")?;

        self.expect_token(Token::If)?;

        self.expect_token(Token::LParen)?;
        let condition: ExpressionTree = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        let true_block = self.parse_block_expression()?;
        let false_block = match self.peek_token()? {
            Token::Else => {
                self.next_token()?;
                Some(self.parse_block_expression()?)
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

        self.finish_parsing(&if_expression)?;

        Ok(if_expression)
    }

    pub fn parse_while_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let start_loc = self.start_parsing("while loop")?;

        self.expect_token(Token::While)?;

        self.expect_token(Token::LParen)?;
        let condition = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        let body = self.parse_block_expression()?;

        let while_loop = WhileExpressionTree {
            loc: start_loc,
            condition,
            body,
        };

        self.finish_parsing(&while_loop)?;

        Ok(ExpressionTree {
            loc: start_loc,
            expression: Expression::While(Box::from(while_loop)),
        })
    }

    pub fn parse_method_call_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing("method call expression")?;
        self.expect_token(Token::Dot)?;

        let expr = ExpressionTree {
            loc: lhs.loc,
            expression: Expression::MethodCall(Box::from(MethodCallExpressionTree {
                loc: lhs.loc,
                receiver: lhs,
                called_method: self.parse_identifier()?,
                params: self.parse_call_params(true)?,
            })),
        };

        self.finish_parsing(&expr)?;
        Ok(expr)
    }

    pub fn parse_field_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing("field expression")?;

        self.expect_token(Token::Dot)?;
        let expr = ExpressionTree {
            loc: lhs.loc,
            expression: Expression::FieldExpression(Box::from(FieldExpressionTree {
                loc: lhs.loc,
                receiver: lhs,
                field: self.parse_identifier()?,
            })),
        };

        self.finish_parsing(&expr)?;

        Ok(expr)
    }

    pub fn parse_call_params(&mut self, allow_self: bool) -> Result<CallParamsTree, ParseError> {
        let start_loc = self.start_parsing("call params")?;

        let mut params = Vec::new();

        self.expect_token(Token::LParen)?;
        while !matches!(self.peek_token()?, Token::RParen) {
            if Self::token_starts_expression(self.peek_token()?) {
                params.push(self.parse_expression()?);
            } else {
                self.unexpected_token(&Self::expression_starters())?;
            }
        }
        self.expect_token(Token::RParen)?;

        let params_tree = CallParamsTree {
            loc: start_loc,
            params,
        };

        self.finish_parsing(&params_tree)?;

        Ok(params_tree)
    }

    pub fn parse_assignment_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing("assignment (rhs)")?;

        self.expect_token(Token::Assign)?;

        let rhs = self.parse_expression()?;

        let assignment = ExpressionTree {
            loc: lhs.loc.clone(),
            expression: Expression::Assignment(AssignmentTree {
                loc: lhs.loc.clone(),
                assignee: Box::from(lhs),
                value: Box::from(rhs),
            }),
        };

        self.finish_parsing(&assignment)?;

        Ok(assignment)
    }

    pub fn parse_binary_expression_min_precedence(
        &mut self,
        lhs: ExpressionTree,
        min_precedence: usize,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing(&format!("expression (min precedence: {})", min_precedence))?;
        let start_loc = lhs.loc;

        let mut expr = lhs;
        while Self::token_is_operator_of_at_least_precedence(&self.peek_token()?, min_precedence) {
            let operation = self.next_token()?;
            let mut rhs = self.parse_primary_expression()?;

            while Self::token_is_operator_of_at_least_precedence(
                &self.peek_token()?,
                ir::BinaryOperations::precedence_of_token(&operation),
            ) {
                rhs = self.parse_binary_expression_min_precedence(
                    rhs,
                    ir::BinaryOperations::precedence_of_token(&operation),
                )?;
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
                    _ => self.unexpected_token(&[
                        Token::Plus,
                        Token::Minus,
                        Token::Star,
                        Token::FSlash,
                        Token::LThan,
                        Token::GThan,
                        Token::LThanE,
                        Token::GThanE,
                        Token::Equals,
                        Token::NotEquals,
                    ])?,
                },
            };
        }

        self.finish_parsing(&expr)?;

        Ok(expr)
    }

    pub fn parse_primary_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let start_loc = self.start_parsing("primary expression")?;

        let primary_expression = ExpressionTree {
            loc: start_loc,
            expression: {
                match self.peek_token()? {
                    Token::Identifier(value) => {
                        self.next_token()?;
                        Expression::Identifier(value)
                    }
                    Token::UnsignedDecimalConstant(value) => {
                        self.next_token()?;
                        Expression::UnsignedDecimalConstant(value)
                    }
                    Token::LParen => {
                        self.next_token()?;
                        let expr = self.parse_expression()?;
                        self.expect_token(Token::RParen)?;
                        expr.expression
                    } // TODO: don't duplciate ExpressionTree here
                    _ => self.unexpected_token(&[
                        Token::Identifier("".into()),
                        Token::UnsignedDecimalConstant(0),
                        Token::LParen,
                    ])?,
                }
            },
        };

        self.finish_parsing(&primary_expression)?;

        Ok(primary_expression)
    }

    pub fn parse_identifier_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let start_loc = self.start_parsing("identifier expression")?;

        let expr = ExpressionTree {
            loc: start_loc,
            expression: Expression::Identifier(self.parse_identifier()?),
        };

        self.finish_parsing(&expr)?;

        Ok(expr)
    }

    pub fn parse_literal_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let start_loc = self.start_parsing("literal expression")?;

        let expr = ExpressionTree {
            loc: start_loc,
            expression: match self.peek_token()? {
                Token::UnsignedDecimalConstant(value) => {
                    self.next_token()?;
                    Expression::UnsignedDecimalConstant(value)
                }
                _ => self.unexpected_token(&[Token::UnsignedDecimalConstant(0)])?,
            },
        };

        self.finish_parsing(&expr)?;

        Ok(expr)
    }
}
