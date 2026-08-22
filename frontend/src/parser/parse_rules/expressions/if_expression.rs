use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, parse_rules::ExpressionParser},
};

impl ExpressionParser<'_, '_> {
    pub(crate) fn parse_if_expression(
        &mut self,
    ) -> Result<ast::expressions::IfExpressionTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("if expression")?;

        let if_keyword_loc = self.expect_token(Token::If)?;

        self.expect_token(Token::LParen)?;
        let condition: ast::Expression = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        let true_block = self.parse_block_expression()?;
        let false_block = match self.peek_token()? {
            Token::Else => {
                self.next_token()?;
                Some(self.parse_block_expression()?)
            }
            _ => None,
        };

        let if_expression = ast::expressions::IfExpressionTree {
            if_keyword_loc,
            condition,
            true_block,
            false_block,
        };

        self.finish_parsing(if_expression)
    }
}
