use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_let_statement(&mut self) -> Result<LetTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("let statement")?;

        self.expect_token(Token::Let)?;
        let mutable = match self.peek_token()? {
            Token::Mut => {
                self.expect_token(Token::Mut)?;
                true
            }
            _ => false,
        };

        let name = self.parse_identifier()?;
        let type_ = match self.peek_token()? {
            Token::Colon => {
                self.expect_token(Token::Colon)?;
                Some(self.parse_type()?)
            }
            _ => None,
        };

        let value = match self.peek_token()? {
            Token::Equals => {
                self.expect_token(Token::Equals)?;
                Some(self.parse_expression()?)
            }
            _ => None,
        };

        let let_tree = LetTree::new(start_loc, name, type_, mutable, value);
        self.finish_parsing(&let_tree)?;
        Ok(let_tree)
    }
}
