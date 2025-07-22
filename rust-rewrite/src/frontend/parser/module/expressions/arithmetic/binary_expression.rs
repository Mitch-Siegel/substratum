use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_binary_expression_min_precedence(
        &mut self,
        lhs: ExpressionTree,
        min_precedence: usize,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing(&format!("expression (min precedence: {})", min_precedence))?;
        let start_loc = lhs.loc.clone();

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
                loc: start_loc.clone(),
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

    pub fn parse_binary_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.parse_binary_expression_min_precedence(lhs, 0)
    }
}
