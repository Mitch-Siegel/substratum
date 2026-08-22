use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, parse_rules::ExpressionParser},
};

impl ExpressionParser<'_, '_> {
    pub(crate) fn precedence_of_token(token: &Token) -> usize {
        match token {
            Token::Plus | Token::Minus => 1,
            Token::Star | Token::FSlash => 2,
            Token::LThan | Token::GThan | Token::LThanE | Token::GThanE => 3,
            Token::Equals | Token::NotEquals => 4,
            _ => {
                panic!("Invalid token {token} passed to BinaryOperations::precedence_of_token");
            }
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
            | Token::NotEquals => Self::precedence_of_token(token) >= precedence,
            _ => false,
        }
    }

    pub(crate) fn parse_binary_expression_min_precedence(
        &mut self,
        lhs: ast::Expression,
        min_precedence: usize,
    ) -> Result<ast::Expression, ParseError> {
        let (_start_loc, _span) =
            self.start_parsing(&format!("expression (min precedence: {min_precedence})"))?;

        let mut expr = lhs;
        while Self::token_is_operator_of_at_least_precedence(&self.peek_token()?, min_precedence) {
            let operation = self.next_token()?;
            let mut rhs = self.parse_primary_expression()?;

            while Self::token_is_operator_of_at_least_precedence(
                &self.peek_token()?,
                Self::precedence_of_token(&operation),
            ) {
                rhs = self.parse_binary_expression_min_precedence(
                    rhs,
                    Self::precedence_of_token(&operation),
                )?;
            }

            let operands = ast::expressions::arithmetic::ArithmeticDualOperands {
                e1: Box::new(expr),
                e2: Box::new(rhs),
            };
            expr = match operation {
                Token::Plus => ast::Expression::Arithmetic(
                    ast::expressions::ArithmeticExpressionTree::Add(operands),
                ),
                Token::Minus => ast::Expression::Arithmetic(
                    ast::expressions::ArithmeticExpressionTree::Subtract(operands),
                ),
                Token::Star => ast::Expression::Arithmetic(
                    ast::expressions::ArithmeticExpressionTree::Multiply(operands),
                ),
                Token::FSlash => ast::Expression::Arithmetic(
                    ast::expressions::ArithmeticExpressionTree::Divide(operands),
                ),
                Token::LThan => ast::Expression::Comparison(
                    ast::expressions::ComparisonExpressionTree::LThan(operands),
                ),
                Token::GThan => ast::Expression::Comparison(
                    ast::expressions::ComparisonExpressionTree::GThan(operands),
                ),
                Token::LThanE => ast::Expression::Comparison(
                    ast::expressions::ComparisonExpressionTree::LThanE(operands),
                ),
                Token::GThanE => ast::Expression::Comparison(
                    ast::expressions::ComparisonExpressionTree::GThanE(operands),
                ),
                Token::Equals => ast::Expression::Comparison(
                    ast::expressions::ComparisonExpressionTree::Equals(operands),
                ),
                Token::NotEquals => ast::Expression::Comparison(
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
            };
        }

        self.finish_parsing(expr)
    }

    pub(crate) fn parse_binary_expression(
        &mut self,
        lhs: ast::Expression,
    ) -> Result<ast::Expression, ParseError> {
        self.parse_binary_expression_min_precedence(lhs, 0)
    }
}
