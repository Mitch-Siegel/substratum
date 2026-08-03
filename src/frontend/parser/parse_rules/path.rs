use crate::frontend::{
    lexer::Token,
    parser::{
        ast,
        parse_rules::{Ast, ParseError, Parser, PathParser},
    },
};

impl PathParser<'_, '_> {
    pub(crate) fn parse_path<P, D>(
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
            starts_global,
            segments,
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
                Token::Identifier(String::new()),
                Token::Super,
                Token::SelfLower,
                Token::SelfUpper,
            ])?,
        };
        self.finish_parsing(segment)
    }
}

#[cfg(test)]
mod tests {
    use crate::frontend::{
        ast::{builder::*, IdentifierTree},
        lexer::Token,
        parser::{tests::test_parser, ParseError, Parser},
    };

    fn no_parse_extra_data(_: &mut Parser) -> Result<Option<IdentifierTree>, ParseError> {
        Ok(None)
    }

    #[test]
    fn parse_path() {
        let mut parser = test_parser("::example::self::of::super::path::Self".into());
        assert_eq!(
            parser.path_parser().parse_path(no_parse_extra_data),
            Ok(path(
                vec![
                    path_ident_segment("example", test_loc(1, 3), None),
                    path_self_lower_segment(test_loc(1, 12), None),
                    path_ident_segment("of", test_loc(1, 18), None),
                    path_super_segment(test_loc(1, 22), None),
                    path_ident_segment("path", test_loc(1, 29), None),
                    path_self_upper_segment(test_loc(1, 35), None),
                ],
                Some(test_span(1, 1, 1, 3))
            ))
        );
    }

    #[test]
    fn parse_path_no_global() {
        let mut parser = test_parser("example::self::of::super::path::Self".into());
        assert_eq!(
            parser.path_parser().parse_path(no_parse_extra_data),
            Ok(path(
                vec![
                    path_ident_segment("example", test_loc(1, 1), None),
                    path_self_lower_segment(test_loc(1, 10), None),
                    path_ident_segment("of", test_loc(1, 16), None),
                    path_super_segment(test_loc(1, 20), None),
                    path_ident_segment("path", test_loc(1, 27), None),
                    path_self_upper_segment(test_loc(1, 33), None),
                ],
                None,
            ))
        );
    }

    #[test]
    fn bad_kw_in_path() {
        let mut parser = test_parser("if path::thing".into());
        assert_eq!(
            parser.path_parser().parse_path(no_parse_extra_data),
            Err(ParseError::unexpected_token(
                test_loc(1, 1),
                Token::If,
                &[
                    Token::Identifier("".into()),
                    Token::Super,
                    Token::SelfLower,
                    Token::SelfUpper
                ],
                "path ident segment".into(),
                test_loc(1, 1),
                test_loc(0, 0),
            ))
        );
    }
}
