use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_primary_expression(&mut self) -> Result<ast::ExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("primary expression")?;

        let primary_expression = match self.peek_token()? {
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
        };

        let expression_tree = ExpressionTree::new(start_loc, primary_expression);
        self.finish_parsing(&expression_tree)?;
        Ok(expression_tree)
    }
}
