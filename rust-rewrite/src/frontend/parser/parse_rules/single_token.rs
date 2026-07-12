use crate::frontend::{lexer::token::Token, *};

use super::{ParseError, Parser};

// parsing functions which only consume a single token
impl<'a> Parser<'a> {
    pub(crate) fn parse_identifier(&mut self) -> Result<ast::IdentifierTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("identifier")?;

        let identifier = match self.peek_token()? {
            Token::Identifier(value) => {
                let loc = self.expect_token(Token::Identifier("".into()))?;
                ast::IdentifierTree { value, loc }
            }
            _ => self.unexpected_token(&[Token::Identifier("".into())])?,
        };

        self.finish_parsing(identifier)
    }
}

#[cfg(test)]
mod tests {
    use crate::frontend::{
        ast::IdentifierTree,
        lexer::{token::Token, Lexer},
        parser::{ParseError, Parser},
        sourceloc::{SourceLoc, SourcePoint, SourceSpan},
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
                loc: SourceSpan::new(
                    String::new(),
                    SourcePoint { line: 1, col: 1 },
                    SourcePoint { line: 1, col: 14 }
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
                SourceLoc::new(String::new(), SourcePoint { line: 1, col: 1 }),
                Token::Struct,
                &[Token::Identifier("".into())],
                "identifier".into(),
                SourceLoc::new(String::new(), SourcePoint { line: 1, col: 1 }),
                SourceLoc::new(
                    String::from("src/frontend/parser/parse_rules/single_token.rs"),
                    SourcePoint { line: 15, col: 23 }
                ),
            ))
        );
    }
}
