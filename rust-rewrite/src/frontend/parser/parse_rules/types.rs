use crate::frontend::ast::types::PrimitiveTypePathTree;
use crate::midend::{self, types};

use crate::frontend::{ast, parser::parse_rules::*};

impl<'a, 'p> TypeParser<'a, 'p> {
    pub fn parse_type(&mut self) -> Result<TypeTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("type")?;

        let type_tree = TypeTree::TypeNoBounds(self.parse_type_no_bounds()?);

        self.finish_parsing(type_tree)
    }

    fn parse_type_no_bounds(&mut self) -> Result<ast::types::TypeNoBounds, ParseError> {
        let (_start_loc, _span) = self.start_parsing("type no bounds")?;

        let type_no_bounds = match self.peek_token()? {
            Token::LParen => self.parse_parenthesized_type_or_tuple()?,
            Token::Reference => {
                ast::types::TypeNoBounds::ReferenceType(self.parse_reference_type()?)
            }
            Token::Identifier(_)
            | Token::U8
            | Token::U16
            | Token::U32
            | Token::U64
            | Token::I8
            | Token::I16
            | Token::I32
            | Token::I64
            | Token::SelfUpper => ast::types::TypeNoBounds::TypePath(self.parse_type_path()?),
            _ => self.unexpected_token(&[Token::LParen])?,
        };

        self.finish_parsing(type_no_bounds)
    }

    fn parse_parenthesized_type_or_tuple(
        &mut self,
    ) -> Result<ast::types::TypeNoBounds, ParseError> {
        let (start_loc, _span) = self.start_parsing("parenthesized type or tuple")?;

        self.expect_token(Token::LParen)?;
        let inner_type = match self.peek_token()? {
            Token::RParen => {
                self.expect_token(Token::RParen)?;
                ast::types::TypeNoBounds::TupleType(Vec::new())
            }
            _ => {
                let inner_type = self.parse_type()?;
                match self.peek_token()? {
                    Token::Comma => self.parse_tuple_type(inner_type)?,
                    Token::RParen => {
                        self.expect_token(Token::RParen)?;
                        ast::types::TypeNoBounds::ParenthesizedType(Box::from(inner_type))
                    }
                    _ => self.unexpected_token(&[Token::Comma, Token::RParen])?,
                }
            }
        };

        self.finish_parsing(inner_type)
    }

    fn parse_tuple_type(
        &mut self,
        first_type: TypeTree,
    ) -> Result<ast::types::TypeNoBounds, ParseError> {
        let (_start_loc, _span) = self.start_parsing("tuple type")?;
        self.expect_token(Token::Comma)?;

        let mut tuple_members = vec![first_type];
        loop {
            match self.peek_token()? {
                Token::RParen => {
                    self.expect_token(Token::RParen)?;
                    break;
                }
                _ => tuple_members.push(self.parse_type()?),
            }
        }

        let tuple_type = ast::types::TypeNoBounds::TupleType(tuple_members);

        self.finish_parsing(tuple_type)
    }

    fn parse_reference_type(&mut self) -> Result<ast::types::ReferenceTypeTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("reference type")?;

        self.expect_token(Token::Reference)?;
        let mutability = match self.peek_token()? {
            Token::Mut => {
                self.expect_token(Token::Mut)?;
                midend::types::Mutability::Mutable
            }
            _ => midend::types::Mutability::Immutable,
        };

        let reference_tree = ast::types::ReferenceTypeTree {
            loc: start_loc,
            mutability,
            type_: Box::new(self.parse_type_no_bounds()?),
        };

        self.finish_parsing(reference_tree)
    }

    fn parse_type_path(&mut self) -> Result<ast::types::TypePath, ParseError> {
        let (start_loc, _span) = self.start_parsing("type path")?;

        let type_path = match self.peek_token()? {
            Token::U8
            | Token::U16
            | Token::U32
            | Token::U64
            | Token::I8
            | Token::I16
            | Token::I32
            | Token::I64 => ast::types::TypePath::Primitive(self.parse_primitive_type_path()?),
            Token::SelfUpper => {
                self.next_token()?;
                ast::types::TypePath::Primitive(ast::types::PrimitiveTypePathTree {
                    loc: start_loc,
                    type_: types::Syntactic::_Self,
                })
            }
            Token::SelfLower | Token::Identifier(_) => {
                ast::types::TypePath::ItemPath(self.parse_item_type_path_tree(false)?)
            }
            Token::PathSep => ast::types::TypePath::ItemPath(self.parse_item_type_path_tree(true)?),
            _ => self.unexpected_token(&[
                Token::U8,
                Token::U16,
                Token::U32,
                Token::U64,
                Token::I8,
                Token::I16,
                Token::I32,
                Token::I64,
                Token::SelfUpper,
                Token::SelfLower,
                Token::Identifier("".into()),
                Token::PathSep,
            ])?,
        };

        self.finish_parsing(type_path)
    }

    fn parse_primitive_type_path(&mut self) -> Result<PrimitiveTypePathTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("type path")?;

        let primitive = match self.peek_token()? {
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
            _ => self.unexpected_token(&[
                Token::U8,
                Token::U16,
                Token::U32,
                Token::U64,
                Token::I8,
                Token::I16,
                Token::I32,
                Token::I64,
            ])?,
        };

        let primitive_path = PrimitiveTypePathTree {
            loc: start_loc,
            type_: primitive,
        };
        self.finish_parsing(primitive_path)
    }

    fn parse_item_type_path_tree(
        &mut self,
        starts_global: bool,
    ) -> Result<ast::types::TypeItemPathTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("path to type item")?;

        let mut require_path_sep = starts_global;

        let mut segments = Vec::new();
        loop {
            if require_path_sep {
                self.expect_token(Token::PathSep)?;
                require_path_sep = false;
            }

            match self.peek_token()? {
                Token::Identifier(_) | Token::Super | Token::SelfLower | Token::SelfUpper => {
                    let ident_segment = self.expression_parser().parse_path_ident_segment()?;
                    let generic_args = match self.peek_token()? {
                        Token::PathSep => {
                            self.expect_token(Token::PathSep)?;
                            self.item_parser().try_parse_generic_args_list()?
                        }
                        _ => None,
                    };

                    if generic_args.is_some() {
                        require_path_sep = true;
                    }

                    let path_segment = ast::types::TypePathSegmentTree {
                        ident_segment,
                        generic_args,
                    };
                    segments.push(path_segment);
                }
                _ => break,
            }
        }

        let type_item_path_tree = ast::types::TypeItemPathTree {
            loc: start_loc,
            starts_global,
            segments,
        };
        self.finish_parsing(type_item_path_tree)
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
