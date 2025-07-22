use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_block_expression(&mut self) -> Result<CompoundExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("compound statement")?;

        self.expect_token(Token::LCurly)?;
        let mut statements: Vec<StatementTree> = Vec::new();
        loop {
            match self.peek_token()? {
                Token::RCurly => break,
                _ => statements.push(self.parse_statement()?),
            }
        }
        self.expect_token(Token::RCurly)?;

        let compound_statement = CompoundExpressionTree {
            loc: start_loc,
            statements: statements,
        };

        self.finish_parsing(&compound_statement)?;

        Ok(compound_statement)
    }
}
