use crate::midend::types::{Mutability, Type};

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

    fn parse_type_inner(&mut self) -> Result<Type, ParseError> {
        let (_start_loc, _span) = self.start_parsing("typename")?;

        let type_ = match self.peek_token()? {
            Token::Reference => {
                self.expect_token(Token::Reference)?;
                let mutability = match self.peek_token()? {
                    Token::Mut => {
                        self.expect_token(Token::Mut)?;
                        Mutability::Mutable
                    }
                    _ => Mutability::Immutable,
                };
                Type::Reference(mutability, Box::new(self.parse_type_inner()?))
            }
            Token::SelfUpper => {
                self.expect_token(Token::SelfUpper)?;
                Type::_Self
            }
            _ => self.parse_type_name()?,
        };

        self.finish_parsing(&type_)?;

        Ok(type_)
    }

    fn parse_type_name(&mut self) -> Result<Type, ParseError> {
        let (_start_loc, _span) = self.start_parsing("type name")?;

        let type_name = match self.peek_token()? {
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
                Type::Named(name)
            }
            Token::SelfLower => {
                self.next_token()?;
                Type::_Self
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
                Token::SelfLower,
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

    #[test]
    fn parse_type_name() {
        let type_names = [
            ("u8", Type::U8),
            ("u16", Type::U16),
            ("u32", Type::U32),
            ("u64", Type::U64),
            ("i8", Type::I8),
            ("i16", Type::I16),
            ("i32", Type::I32),
            ("i64", Type::I64),
            ("MyStruct", Type::Named("MyStruct".into())),
            ("self", Type::_Self),
        ];

        for (string, type_) in type_names {
            let mut p = Parser::new(Lexer::from_string(string));
            assert_eq!(p.parse_type_name(), Ok(type_));
        }
    }

    #[test]
    fn parse_type_name_error() {
        let mut p = Parser::new(Lexer::from_string("123"));
        assert_eq!(
            p.parse_type_name(),
            Err(ParseError::unexpected_token(
                SourceLoc::new(1, 1),
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
                    Token::SelfLower,
                ],
                "type name".into(),
                SourceLoc::new(1, 1),
            ))
        );
    }

    #[test]
    fn parse_type_inner() {
        let types = [
            ("u32", Type::U32),
            (
                "&u32",
                Type::Reference(Mutability::Immutable, Box::from(Type::U32)),
            ),
            (
                "&mut u32",
                Type::Reference(Mutability::Mutable, Box::from(Type::U32)),
            ),
            ("Self", Type::_Self),
            (
                "&Self",
                Type::Reference(Mutability::Immutable, Box::from(Type::_Self)),
            ),
            (
                "&mut Self",
                Type::Reference(Mutability::Mutable, Box::from(Type::_Self)),
            ),
        ];

        for (string, type_) in types {
            let mut p = Parser::new(Lexer::from_string(string));
            assert_eq!(p.parse_type_inner(), Ok(type_));
        }
    }

    #[test]
    fn parse_type() {
        let mut p = Parser::new(Lexer::from_string("u32"));
        assert_eq!(
            p.parse_type(),
            Ok(TypeTree::new(SourceLoc::new(1, 1), Type::U32))
        );
    }
}
