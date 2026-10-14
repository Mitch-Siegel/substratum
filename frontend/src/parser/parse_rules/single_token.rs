use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, Parser},
};

// parsing functions which only consume a single token
impl Parser<'_> {
    pub(crate) fn parse_identifier(&mut self) -> Result<ast::IdentifierTree, ParseError> {
        let (_start, _span) = self.start_parsing("identifier")?;

        let identifier = match self.peek_token()? {
            Token::Identifier(value) => {
                let span = self.expect_token(Token::Identifier(String::new()))?;
                ast::IdentifierTree { span, value }
            }
            _ => self.unexpected_token(&[Token::Identifier(String::new())])?,
        };

        self.finish_parsing(identifier)
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        ast::IdentifierTree,
        lexer::{Lexer, token::Token},
        loc::{Loc, Point, Span},
        parser::{ParseError, Parser},
    };
    use std::path::Path;

    #[test]
    fn parse_identifier() {
        let mut p = Parser::new(
            "".into(),
            Path::new(""),
            Lexer::from_string("my_identifier"),
        );
        assert_eq!(
            p.parse_identifier(),
            Ok(IdentifierTree {
                loc: Span::new(
                    String::new(),
                    Point { line: 1, col: 1 },
                    Point { line: 1, col: 14 }
                ),
                value: "my_identifier".into()
            })
        );
    }

    #[test]
    fn parse_identifier_error() {
        let mut p = Parser::new("".into(), Path::new(""), Lexer::from_string("struct"));
        assert_eq!(
            p.parse_identifier(),
            Err(ParseError::unexpected_token(
                Loc::new(String::new(), Point { line: 1, col: 1 }),
                Token::Struct,
                &[Token::Identifier("".into())],
                "identifier".into(),
                Loc::new(String::new(), Point { line: 1, col: 1 }),
                Loc::new(
                    String::from("src/frontend/parser/parse_rules/single_token.rs"),
                    Point { line: 15, col: 23 }
                ),
            ))
        );
    }
}
