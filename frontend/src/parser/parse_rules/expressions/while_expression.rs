use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, parse_rules::ExpressionParser},
};

impl ExpressionParser<'_, '_> {
    pub(crate) fn parse_while_expression(
        &mut self,
    ) -> Result<ast::expressions::WhileExpressionTree, ParseError> {
        let (_start, _span) = self.start_parsing("while loop")?;

        let while_keyword = self.expect_token(Token::While)?;

        self.expect_token(Token::LParen)?;
        let condition = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        let body = self.parse_block_expression()?;

        let while_loop = ast::expressions::WhileExpressionTree {
            while_keyword,
            condition,
            body,
        };

        self.finish_parsing(while_loop)
    }
}
