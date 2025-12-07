use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_literal_expression(&mut self) -> Result<ast::Expression, ParseError> {
        let (_start_loc, _span) = self.start_parsing("literal expression")?;

        let literal_expression = match self.peek_token()? {
            Token::UnsignedDecimalConstant(value) => {
                let loc = self.expect_token(Token::UnsignedDecimalConstant(0))?;
                ast::expressions::Expression::UnsignedDecimalConstant(loc, value)
            }
            _ => self.unexpected_token(&[Token::UnsignedDecimalConstant(0)])?,
        };

        self.finish_parsing(literal_expression)
    }
}
