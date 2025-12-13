use crate::frontend::{
    lexer::Token,
    parser::{ast, parse_rules::*},
};

impl<'a, 'p> PathParser<'a, 'p> {
    pub fn parse_path<P, D>(
        &mut self,
        try_parse_segment_data: P,
    ) -> Result<ast::path::PathTree<D>, ParseError>
    where
        P: Fn(&mut Parser) -> Result<Option<D>, ParseError>,
        D: Ast + std::fmt::Debug,
    {
        let (_start_loc, _span) = self.start_parsing("path")?;

        let starts_global = match self.peek_token()? {
            Token::PathSep => Some(self.expect_token(Token::PathSep)?),
            _ => None,
        };

        let mut segments = Vec::new();

        loop {
            segments.push(self.parse_segment(&try_parse_segment_data)?);
            match self.peek_token()? {
                Token::PathSep => {
                    self.expect_token(Token::PathSep)?;
                }
                _ => break,
            }
        }

        let path = ast::path::PathTree {
            segments,
            starts_global,
        };
        self.finish_parsing(path)
    }

    fn parse_segment<P, D>(
        &mut self,
        try_parse_segment_data: P,
    ) -> Result<ast::path::PathSegmentTree<D>, ParseError>
    where
        P: Fn(&mut Parser) -> Result<Option<D>, ParseError>,
        D: Ast + std::fmt::Debug,
    {
        let (_start_loc, _span) = self.start_parsing("path segment")?;

        let ident = self.parse_ident_segment()?;
        let data = try_parse_segment_data(self.0)?;

        let segment = ast::path::PathSegmentTree { ident, data };
        self.finish_parsing(segment)
    }

    fn parse_ident_segment(&mut self) -> Result<ast::path::IdentSegment, ParseError> {
        let (_start_loc, _span) = self.start_parsing("path ident segment")?;

        let segment = match self.peek_token()? {
            Token::Identifier(_) => ast::path::IdentSegment::Ident(self.parse_identifier()?),
            Token::Super => ast::path::IdentSegment::Super(self.expect_token(Token::Super)?),
            Token::SelfLower => {
                ast::path::IdentSegment::SelfLower(self.expect_token(Token::SelfLower)?)
            }
            Token::SelfUpper => {
                ast::path::IdentSegment::SelfUpper(self.expect_token(Token::SelfUpper)?)
            }
            _ => self.unexpected_token(&[
                Token::Identifier("".into()),
                Token::Super,
                Token::SelfLower,
                Token::SelfUpper,
            ])?,
        };
        self.finish_parsing(segment)
    }
}
