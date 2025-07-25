use crate::frontend::parser::{parse_rules::*, *};

impl<'a, 'p> ExpressionParser<'a, 'p> {
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

            let operands = ast::expressions::arithmetic::ArithmeticDualOperands {
                e1: Box::new(expr),
                e2: Box::new(rhs),
            };
            expr = ExpressionTree {
                loc: start_loc.clone(),
                expression: match operation {
                    Token::Plus => Expression::Arithmetic(
                        ast::expressions::ArithmeticExpressionTree::Add(operands),
                    ),
                    Token::Minus => Expression::Arithmetic(
                        ast::expressions::ArithmeticExpressionTree::Subtract(operands),
                    ),
                    Token::Star => Expression::Arithmetic(
                        ast::expressions::ArithmeticExpressionTree::Multiply(operands),
                    ),
                    Token::FSlash => Expression::Arithmetic(
                        ast::expressions::ArithmeticExpressionTree::Divide(operands),
                    ),
                    Token::LThan => Expression::Comparison(
                        ast::expressions::ComparisonExpressionTree::LThan(operands),
                    ),
                    Token::GThan => Expression::Comparison(
                        ast::expressions::ComparisonExpressionTree::GThan(operands),
                    ),
                    Token::LThanE => Expression::Comparison(
                        ast::expressions::ComparisonExpressionTree::LThanE(operands),
                    ),
                    Token::GThanE => Expression::Comparison(
                        ast::expressions::ComparisonExpressionTree::GThanE(operands),
                    ),
                    Token::Equals => Expression::Comparison(
                        ast::expressions::ComparisonExpressionTree::Equals(operands),
                    ),
                    Token::NotEquals => Expression::Comparison(
                        ast::expressions::ComparisonExpressionTree::NotEquals(operands),
                    ),
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
