use crate::frontend::parser::parse_rules::*;

impl<'a> Parser<'a> {
    pub fn parse_block_expression(
        &mut self,
    ) -> Result<ast::expressions::BlockExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("compound statement")?;

        let open_brace_loc = self.expect_token(Token::LCurly)?;
        let mut statements: Vec<StatementTree> = Vec::new();
        loop {
            match self.peek_token()? {
                Token::RCurly => break,
                _ => statements.push(self.statement_parser().parse_statement()?),
            }
        }
        let close_brace_loc = self.expect_token(Token::RCurly)?;

        let compound_statement = ast::expressions::BlockExpressionTree {
            open_brace_loc,
            statements: statements,
            close_brace_loc,
        };

        self.finish_parsing(compound_statement)
    }
}
