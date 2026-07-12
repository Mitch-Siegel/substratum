use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> StatementParser<'a, 'p> {
    pub(crate) fn parse_let_statement(&mut self) -> Result<ast::statements::LetTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("let statement")?;

        let let_keyword_loc = self.expect_token(Token::Let)?;

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
                Some(self.type_parser().parse_type()?)
            }
            _ => None,
        };

        self.expect_token(Token::Assign)?;
        let value = self.expression_parser().parse_expression()?;

        let let_tree = ast::statements::LetTree {
            let_keyword_loc,
            name,
            type_,
            mutable,
            value,
        };
        self.finish_parsing(let_tree)
    }
}
