use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, parse_rules::ExpressionParser},
};

impl ExpressionParser<'_, '_> {
    pub(crate) fn parse_parenthesized_expression(&mut self) -> Result<ast::Expression, ParseError> {
        let (_start_loc, _span) = self.start_parsing("parenthesized expression")?;

        self.expect_token(Token::LParen)?;
        let parenthesized_expr = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        self.finish_parsing(parenthesized_expr)
    }
}
