use crate::midend::{self, types};

use crate::frontend::{ast, parser::parse_rules::*};

impl<'a, 'p> TypeParser<'a, 'p> {
    pub fn parse_type(&mut self) -> Result<TypeTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("type")?;

        let type_tree = TypeTree::TypeNoBounds(self.parse_type_no_bounds()?);

        self.finish_parsing(type_tree)
    }

    fn parse_type_no_bounds(&mut self) -> Result<ast::types::TypeNoBoundsTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("type no bounds")?;

        let type_no_bounds = match self.peek_token()? {
            Token::Identifier(_)
            | Token::U8
            | Token::U16
            | Token::U32
            | Token::U64
            | Token::I8
            | Token::I16
            | Token::I32
            | Token::I64
            | Token::SelfUpper => ast::types::TypeNoBoundsTree::TypePath(self.parse_type_path()?),
            Token::LParen => self.parse_parenthesized_type_or_tuple()?,
            Token::Reference => {
                ast::types::TypeNoBoundsTree::ReferenceType(self.parse_reference_type()?)
            }
            Token::LBracket => ast::types::TypeNoBoundsTree::ArrayType(self.parse_array_type()?),

            _ => self.unexpected_token(&[
                Token::Identifier("".into()),
                Token::U8,
                Token::U16,
                Token::U32,
                Token::U64,
                Token::I8,
                Token::I16,
                Token::I32,
                Token::I64,
                Token::SelfUpper,
                Token::LParen,
                Token::Reference,
                Token::LBracket,
            ])?,
        };

        self.finish_parsing(type_no_bounds)
    }

    fn parse_parenthesized_type_or_tuple(
        &mut self,
    ) -> Result<ast::types::TypeNoBoundsTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("parenthesized type or tuple")?;

        let open_paren_loc = self.expect_token(Token::LParen)?;
        let inner_type = match self.peek_token()? {
            Token::RParen => {
                let close_paren_loc = self.expect_token(Token::RParen)?;
                ast::types::TypeNoBoundsTree::TupleType(ast::types::TupleTypeTree {
                    open_paren_loc,
                    members: Vec::new(),
                    close_paren_loc,
                })
            }
            _ => {
                let following_type = self.parse_type()?;
                match self.peek_token()? {
                    Token::Comma => ast::types::TypeNoBoundsTree::TupleType(
                        self.parse_tuple_type(open_paren_loc, following_type)?,
                    ),
                    Token::RParen => ast::types::TypeNoBoundsTree::ParenthesizedType(
                        self.parse_parenthesized_type(open_paren_loc, following_type)?,
                    ),
                    _ => self.unexpected_token(&[Token::Comma, Token::RParen])?,
                }
            }
        };

        self.finish_parsing(inner_type)
    }

    fn parse_tuple_type(
        &mut self,
        open_paren_loc: sourceloc::SourceSpan,
        first_type: TypeTree,
    ) -> Result<ast::types::TupleTypeTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("tuple type")?;
        self.expect_token(Token::Comma)?;

        let mut members = vec![first_type];
        loop {
            match self.peek_token()? {
                Token::RParen => {
                    break;
                }
                _ => members.push(self.parse_type()?),
            }

            match self.peek_token()? {
                Token::Comma => {
                    self.expect_token(Token::Comma)?;
                }
                _ => (),
            }
        }
        let close_paren_loc = self.expect_token(Token::RParen)?;

        let tuple_type = ast::types::TupleTypeTree {
            open_paren_loc,
            members,
            close_paren_loc,
        };

        self.finish_parsing(tuple_type)
    }

    fn parse_parenthesized_type(
        &mut self,
        open_paren_loc: sourceloc::SourceSpan,
        inner_type: ast::types::TypeTree,
    ) -> Result<ast::types::ParenthesizedTypeTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("parenthesized type")?;

        let close_paren_loc = self.expect_token(Token::RParen)?;
        let parenthesized_type = ast::types::ParenthesizedTypeTree {
            open_paren_loc,
            inner_type: Box::from(inner_type),
            close_paren_loc,
        };

        self.finish_parsing(parenthesized_type)
    }

    fn parse_reference_type(&mut self) -> Result<ast::types::ReferenceTypeTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("reference type")?;

        let reference_token_loc = self.expect_token(Token::Reference)?;
        let mutability = match self.peek_token()? {
            Token::Mut => {
                self.expect_token(Token::Mut)?;
                midend::types::Mutability::Mutable
            }
            _ => midend::types::Mutability::Immutable,
        };

        let reference_tree = ast::types::ReferenceTypeTree {
            reference_token_loc,
            mutability,
            type_: Box::new(self.parse_type_no_bounds()?),
        };

        self.finish_parsing(reference_tree)
    }

    fn parse_type_path(&mut self) -> Result<ast::types::TypePath, ParseError> {
        let (_start_loc, _span) = self.start_parsing("type path")?;

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
                let loc = self.expect_token(Token::SelfUpper)?;
                ast::types::TypePath::Primitive(ast::types::PrimitiveTypePathTree {
                    loc,
                    type_: types::Syntactic::_Self,
                })
            }
            Token::SelfLower | Token::Identifier(_) | Token::PathSep => {
                ast::types::TypePath::ItemPath(self.parse_item_type_path_tree()?)
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
                Token::SelfUpper,
                Token::SelfLower,
                Token::Identifier("".into()),
                Token::PathSep,
            ])?,
        };

        self.finish_parsing(type_path)
    }

    fn parse_primitive_type_path(
        &mut self,
    ) -> Result<ast::types::PrimitiveTypePathTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("type path")?;

        let (loc, primitive) = match self.peek_token()? {
            Token::U8 => (self.expect_token(Token::U8)?, types::Syntactic::U8),
            Token::U16 => (self.expect_token(Token::U16)?, types::Syntactic::U16),
            Token::U32 => (self.expect_token(Token::U32)?, types::Syntactic::U32),
            Token::U64 => (self.expect_token(Token::U64)?, types::Syntactic::U64),
            Token::I8 => (self.expect_token(Token::I8)?, types::Syntactic::I8),
            Token::I16 => (self.expect_token(Token::I16)?, types::Syntactic::I16),
            Token::I32 => (self.expect_token(Token::I32)?, types::Syntactic::I32),
            Token::I64 => (self.expect_token(Token::I64)?, types::Syntactic::I64),
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

        let primitive_path = ast::types::PrimitiveTypePathTree {
            loc,
            type_: primitive,
        };
        self.finish_parsing(primitive_path)
    }

    fn try_parse_type_item_path_segment_data(
        parser: &mut Parser,
    ) -> Result<Option<ast::types::TypePathSegmentData>, ParseError> {
        let (_start_loc, _span) = parser.start_parsing("optional path segment data")?;

        let maybe_data = match parser.peek_token()? {
            Token::PathSep => match parser.lookahead_token(1)? {
                Token::LThan => {
                    parser.expect_token(Token::PathSep)?;
                    Some(ast::types::TypePathSegmentData::GenericArgs(
                        parser.item_parser().parse_generic_args_list()?,
                    ))
                }
                _ => None,
            },
            Token::LThan => Some(ast::types::TypePathSegmentData::GenericArgs(
                parser.item_parser().parse_generic_args_list()?,
            )),
            _ => None,
        };

        parser.finish_parsing(maybe_data)
    }

    fn parse_item_type_path_tree(&mut self) -> Result<ast::types::TypeItemPathTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("path to type item")?;
        let underlying_path = self
            .path_parser()
            .parse_path(Self::try_parse_type_item_path_segment_data)?;

        let type_item_path_tree = ast::types::TypeItemPathTree { underlying_path };
        self.finish_parsing(type_item_path_tree)
    }

    fn parse_array_type(&mut self) -> Result<ast::types::ArrayTypeTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("array type")?;

        let open_bracket_loc = self.expect_token(Token::LBracket)?;
        let inner_type = Box::new(self.parse_type()?);
        self.expect_token(Token::Semicolon)?;
        let array_size = self.expression_parser().parse_expression()?;
        let close_bracket_loc = self.expect_token(Token::RBracket)?;

        let array_tree = ast::types::ArrayTypeTree {
            open_bracket_loc,
            inner_type,
            array_size,
            close_bracket_loc,
        };
        self.finish_parsing(array_tree)
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        frontend::{
            ast::{
                builder,
                types::{
                    ArrayTypeTree, ParenthesizedTypeTree, PrimitiveTypePathTree, TupleTypeTree,
                    TypeNoBoundsTree, TypePath, TypeTree,
                },
            },
            parser::tests::test_parser,
        },
        midend::types::{Mutability, Syntactic},
    };

    #[test]
    fn parse_parenthesized_type() {
        let mut p = test_parser("(u64)".into());
        assert_eq!(
            p.type_parser().parse_parenthesized_type_or_tuple(),
            Ok(TypeNoBoundsTree::ParenthesizedType(ParenthesizedTypeTree {
                open_paren_loc: builder::test_span(1, 1, 1, 2).into(),
                inner_type: Box::new(TypeTree::TypeNoBounds(TypeNoBoundsTree::TypePath(
                    TypePath::Primitive(PrimitiveTypePathTree {
                        loc: builder::test_span(1, 2, 1, 5),
                        type_: Syntactic::U64
                    })
                ))),
                close_paren_loc: builder::test_span(1, 5, 1, 6).into(),
            }))
        );
    }

    #[test]
    fn parse_tuple_type() {
        let mut p = test_parser("(u64, MyStruct, i8)".into());
        assert_eq!(
            p.type_parser().parse_parenthesized_type_or_tuple(),
            Ok(TypeNoBoundsTree::TupleType(TupleTypeTree {
                open_paren_loc: builder::test_span(1, 1, 1, 2),
                members: vec![
                    TypeTree::TypeNoBounds(builder::primitive_type(1, 2, Syntactic::U64)),
                    builder::named_type(1, 7, "MyStruct"),
                    TypeTree::TypeNoBounds(builder::primitive_type(1, 17, Syntactic::I8))
                ],
                close_paren_loc: builder::test_span(1, 19, 1, 20),
            }))
        );
    }

    #[test]
    fn parse_single_element_tuple_type() {
        let mut p = test_parser("(u64, )".into());
        assert_eq!(
            p.type_parser().parse_parenthesized_type_or_tuple(),
            Ok(TypeNoBoundsTree::TupleType(TupleTypeTree {
                open_paren_loc: builder::test_span(1, 1, 1, 2),
                members: vec![TypeTree::TypeNoBounds(builder::primitive_type(
                    1,
                    2,
                    Syntactic::U64
                )),],
                close_paren_loc: builder::test_span(1, 7, 1, 8),
            }))
        );
    }

    #[test]
    fn parse_reference_type() {
        let mut p = test_parser("&mut i32".into());
        assert_eq!(
            p.type_parser().parse_reference_type(),
            Ok(builder::reference_of_type(
                1,
                1,
                Mutability::Mutable,
                builder::primitive_type(1, 6, Syntactic::I32)
            ))
        );
    }

    #[test]
    fn parse_array_type() {
        let mut p = test_parser("[u8; 45]".into());
        assert_eq!(
            p.type_parser().parse_array_type(),
            Ok(ArrayTypeTree {
                open_bracket_loc: builder::test_span(1, 1, 1, 2),
                inner_type: Box::new(TypeTree::TypeNoBounds(builder::primitive_type(
                    1,
                    2,
                    Syntactic::U8
                ))),
                array_size: builder::unsigned_decimal_constant(1, 6, 45),
                close_bracket_loc: builder::test_span(1, 8, 1, 9),
            })
        );
    }
}
