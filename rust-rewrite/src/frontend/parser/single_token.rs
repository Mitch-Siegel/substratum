use crate::{
    frontend::{ast::*, lexer::token::Token},
    midend::types::Type,
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

    pub fn parse_typename(&mut self) -> Result<TypenameTree, ParseError> {
        let start_loc = self.start_parsing("typename")?;

        let typename = TypenameTree {
            loc: start_loc,
            type_: match self.peek_token()? {
                Token::U8 => {
                    self.next_token()?;
                    Type::U8
                }
                Token::U16 => {
                    self.next_token()?;
                    Type::U16
                }
                Token::U32 => {
                    self.next_token()?;
                    Type::U32
                }
                Token::U64 => {
                    self.next_token()?;
                    Type::U64
                }
                Token::I8 => {
                    self.next_token()?;
                    Type::I8
                }
                Token::I16 => {
                    self.next_token()?;
                    Type::I16
                }
                Token::I32 => {
                    self.next_token()?;
                    Type::I32
                }
                Token::I64 => {
                    self.next_token()?;
                    Type::I64
                }
                Token::Identifier(name) => {
                    self.next_token()?;
                    Type::UDT(name)
                }
                _ => self.unexpected_token(&[
                    Token::U8,
                    Token::U16,
                    Token::U32,
                    Token::U64,
                    Token::I8,
                    Token::I16,
                    Token::I32,
                    Token::I64,
                    Token::Identifier("".into()),
                ])?,
            },
        };

        self.finish_parsing(&typename)?;

        Ok(typename)
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
