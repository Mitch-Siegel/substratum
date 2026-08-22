use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, parse_rules::ExpressionParser},
};

impl ExpressionParser<'_, '_> {
    pub(crate) fn parse_primary_expression(&mut self) -> Result<ast::Expression, ParseError> {
        let (_start_loc, _span) = self.start_parsing("primary expression")?;

        let primary_expression = match self.peek_token()? {
            Token::Identifier(_) => self.parse_expression()?,
            Token::UnsignedDecimalConstant(value) => {
                let (_, loc) = self.next_token_with_loc()?;
                ast::Expression::UnsignedDecimalConstant(loc, value)
            }
            Token::LParen => {
                self.next_token()?;
                let expr = self.parse_expression()?;
                self.expect_token(Token::RParen)?;
                expr
            }
            _ => self.unexpected_token(&[
                Token::Identifier(String::new()),
                Token::UnsignedDecimalConstant(0),
                Token::LParen,
            ])?,
        };

        self.finish_parsing(primary_expression)
    }
}
