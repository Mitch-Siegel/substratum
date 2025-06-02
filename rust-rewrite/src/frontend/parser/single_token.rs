use crate::{
    frontend::{ast::*, lexer::token::Token},
    midend::types::{Mutability, Type},
};

use super::{ParseError, Parser};

// parsing functions which only consume a single token
impl<'a> Parser<'a> {
    pub fn parse_identifier(&mut self) -> Result<String, ParseError> {
        let _start_loc = self.start_parsing("identifier")?;

        let identifier = match self.expect_token(Token::Identifier(String::from("")))? {
            Token::Identifier(value) => value,
            _ => self.unexpected_token::<String>(&[Token::Identifier("".into())])?,
        };

        self.finish_parsing(&identifier)?;

        Ok(identifier)
    }
}

#[cfg(test)]
mod tests {
    use crate::frontend::{
        lexer::{token::Token, Lexer},
        parser::{ParseError, Parser},
        sourceloc::SourceLoc,
    };

    #[test]
    fn parse_identifier() {
        let mut p = Parser::new(Lexer::from_string("my_identifier"));
        assert_eq!(p.parse_identifier(), Ok("my_identifier".into()));
    }

    #[test]
    fn parse_identifier_error() {
        let mut p = Parser::new(Lexer::from_string("struct"));
        assert_eq!(
            p.parse_identifier(),
            Err(ParseError::unexpected_token(
                SourceLoc::new(1, 1),
                Token::Struct,
                &[Token::Identifier("".into())]
            ))
        );
    }
}
