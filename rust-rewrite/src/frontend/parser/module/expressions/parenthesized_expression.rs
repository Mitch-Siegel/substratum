use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_parenthesized_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("parenthesized expression")?;

        self.expect_token(Token::LParen)?;
        let parenthesized_expr = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        self.finish_parsing(&parenthesized_expr)?;
        Ok(parenthesized_expr)
    }
}
