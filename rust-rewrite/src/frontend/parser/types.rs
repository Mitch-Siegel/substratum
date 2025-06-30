use crate::midend::types::{Mutability, ResolvedType};

use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_type(&mut self) -> Result<TypeTree, ParseError> {
        let (start_loc, _) = self.start_parsing("type")?;

        let type_tree = TypeTree {
            loc: start_loc,
            type_: self.parse_type_inner()?,
        };

        self.finish_parsing(&type_tree)?;

        Ok(type_tree)
    }

    fn parse_type_inner(&mut self) -> Result<ResolvedType, ParseError> {
        let (_start_loc, _) = self.start_parsing("typename")?;

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
                ResolvedType::Reference(mutability, Box::new(self.parse_type_inner()?))
            }
            Token::SelfUpper => {
                self.expect_token(Token::SelfUpper)?;
                ResolvedType::_Self
            }
            _ => self.parse_type_name()?,
        };

        self.finish_parsing(&type_)?;

        Ok(type_)
    }

    fn parse_type_name(&mut self) -> Result<ResolvedType, ParseError> {
        let (_start_loc, _) = self.start_parsing("type name")?;

        let type_name = match self.peek_token()? {
            Token::U8 => {
                self.next_token()?;
                ResolvedType::U8
            }
            Token::U16 => {
                self.next_token()?;
                ResolvedType::U16
            }
            Token::U32 => {
                self.next_token()?;
                ResolvedType::U32
            }
            Token::U64 => {
                self.next_token()?;
                ResolvedType::U64
            }
            Token::I8 => {
                self.next_token()?;
                ResolvedType::I8
            }
            Token::I16 => {
                self.next_token()?;
                ResolvedType::I16
            }
            Token::I32 => {
                self.next_token()?;
                ResolvedType::I32
            }
            Token::I64 => {
                self.next_token()?;
                ResolvedType::I64
            }
            Token::Identifier(name) => {
                self.next_token()?;
                ResolvedType::UDT(name)
            }
            Token::SelfLower => {
                self.next_token()?;
                ResolvedType::_Self
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
    use crate::midend::types::{Mutability, ResolvedType};

    #[test]
    fn parse_type_name() {
        let type_names = [
            ("u8", ResolvedType::U8),
            ("u16", ResolvedType::U16),
            ("u32", ResolvedType::U32),
            ("u64", ResolvedType::U64),
            ("i8", ResolvedType::I8),
            ("i16", ResolvedType::I16),
            ("i32", ResolvedType::I32),
            ("i64", ResolvedType::I64),
            ("MyStruct", ResolvedType::UDT("MyStruct".into())),
            ("self", ResolvedType::_Self),
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
            ("u32", ResolvedType::U32),
            (
                "&u32",
                ResolvedType::Reference(Mutability::Immutable, Box::from(ResolvedType::U32)),
            ),
            (
                "&mut u32",
                ResolvedType::Reference(Mutability::Mutable, Box::from(ResolvedType::U32)),
            ),
            ("Self", ResolvedType::_Self),
            (
                "&Self",
                ResolvedType::Reference(Mutability::Immutable, Box::from(ResolvedType::_Self)),
            ),
            (
                "&mut Self",
                ResolvedType::Reference(Mutability::Mutable, Box::from(ResolvedType::_Self)),
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
            Ok(TypeTree::new(SourceLoc::new(1, 1), ResolvedType::U32))
        );
    }
}
