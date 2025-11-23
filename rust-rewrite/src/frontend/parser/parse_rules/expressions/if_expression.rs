use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_if_expression(
        &mut self,
    ) -> Result<ast::expressions::IfExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("if expression")?;

        self.expect_token(Token::If)?;

        self.expect_token(Token::LParen)?;
        let condition: Expression = self.parse_expression()?;
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
            loc: start_loc,
            condition,
            true_block,
            false_block,
        };

        self.finish_parsing(if_expression)
    }
}
