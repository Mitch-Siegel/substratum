use crate::midend::types;

use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_type(&mut self) -> Result<TypeTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("type")?;

        let type_tree = TypeTree {
            loc: start_loc,
            type_: self.parse_type_inner()?,
        };

        self.finish_parsing(&type_tree)?;

        Ok(type_tree)
    }

    fn parse_type_inner(&mut self) -> Result<types::Syntactic, ParseError> {
        let (_start_loc, _span) = self.start_parsing("typename")?;

        let type_ = match self.peek_token()? {
            Token::Reference => {
                self.expect_token(Token::Reference)?;
                let mutability = match self.peek_token()? {
                    Token::Mut => {
                        self.expect_token(Token::Mut)?;
                        types::Mutability::Mutable
                    }
                    _ => types::Mutability::Immutable,
                };
                types::Syntactic::Reference(mutability, Box::new(self.parse_type_inner()?))
            }
            Token::SelfUpper => {
                self.expect_token(Token::SelfUpper)?;
                types::Syntactic::_Self
            }
            _ => self.parse_type_name()?,
        };

        self.finish_parsing(&type_)?;

        Ok(type_)
    }

    fn parse_type_name(&mut self) -> Result<types::Syntactic, ParseError> {
        let (_start_loc, _span) = self.start_parsing("type name")?;

        let type_name = match self.peek_token()? {
            Token::U8 => {
                self.next_token()?;
                types::Syntactic::U8
            }
            Token::U16 => {
                self.next_token()?;
                types::Syntactic::U16
            }
            Token::U32 => {
                self.next_token()?;
                types::Syntactic::U32
            }
            Token::U64 => {
                self.next_token()?;
                types::Syntactic::U64
            }
            Token::I8 => {
                self.next_token()?;
                types::Syntactic::I8
            }
            Token::I16 => {
                self.next_token()?;
                types::Syntactic::I16
            }
            Token::I32 => {
                self.next_token()?;
                types::Syntactic::I32
            }
            Token::I64 => {
                self.next_token()?;
                types::Syntactic::I64
            }
            Token::Identifier(name) => {
                self.next_token()?;
                types::Syntactic::Named(name)
            }
            Token::SelfUpper => {
                self.next_token()?;
                types::Syntactic::_Self
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
                Token::SelfUpper,
            ])?,
        };

        self.finish_parsing(&type_name)?;

        Ok(type_name)
    }
}

#[cfg(test)]
mod tests {
    use crate::frontend::parser::*;
    use crate::midend::types::{Mutability, Type};
    use std::path::Path;

    #[test]
    fn parse_type_name() {
        let type_names = [
            ("u8", types::Syntactic::U8),
            ("u16", types::Syntactic::U16),
            ("u32", types::Syntactic::U32),
            ("u64", types::Syntactic::U64),
            ("i8", types::Syntactic::I8),
            ("i16", types::Syntactic::I16),
            ("i32", types::Syntactic::I32),
            ("i64", types::Syntactic::I64),
            ("MyStruct", types::Syntactic::Named("MyStruct".into())),
            ("Self", types::Syntactic::_Self),
        ];

        for (string, type_) in type_names {
            let mut p = Parser::new("".into(), Path::new(""), Lexer::from_string(string));
            assert_eq!(p.parse_type_name(), Ok(type_));
        }
    }

    #[test]
    fn parse_type_name_error() {
        let mut p = Parser::new("".into(), Path::new(""), Lexer::from_string("123"));
        assert_eq!(
            p.parse_type_name(),
            Err(ParseError::unexpected_token(
                SourceLoc::new(Path::new(""), 1, 1),
                Token::UnsignedDecimalConstant(123),
                &[
                    Token::U8,
                    Token::U16,
                    Token::U32,
                    Token::U64,
                    Token::I8,
                    Token::I16,
                    Token::I32,
                    Token::I64,
                    Token::Identifier("".into()),
                    Token::SelfUpper,
                ],
                "type name".into(),
                SourceLoc::new(Path::new(""), 1, 1),
                SourceLoc::new(
                    Path::new("src/frontend/parser/parse_rules/types.rs"),
                    90,
                    23
                ),
            ))
        );
    }

    #[test]
    fn parse_type_inner() {
        let types = [
            ("u32", types::Syntactic::U32),
            (
                "&u32",
                types::Syntactic::Reference(Mutability::Immutable, Box::from(Type::U32)),
            ),
            (
                "&mut u32",
                types::Syntactic::Reference(Mutability::Mutable, Box::from(Type::U32)),
            ),
            ("Self", types::Syntactic::_Self),
            (
                "&Self",
                types::Syntactic::Reference(Mutability::Immutable, Box::from(Type::_Self)),
            ),
            (
                "&mut Self",
                types::Syntactic::Reference(Mutability::Mutable, Box::from(Type::_Self)),
            ),
        ];

        for (string, type_) in types {
            let mut p = Parser::new("".into(), Path::new(""), Lexer::from_string(string));
            assert_eq!(p.parse_type_inner(), Ok(type_));
        }
    }

    #[test]
    fn parse_type() {
        let mut p = Parser::new("".into(), Path::new(""), Lexer::from_string("u32"));
        assert_eq!(
            p.parse_type(),
            Ok(TypeTree::new(
                SourceLoc::new(Path::new(""), 1, 1),
                types::Syntactic::U32
            ))
        );
    }
}
