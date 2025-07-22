use crate::frontend::{ast::*, lexer::token::Token};
use crate::midend;

use super::{ParseError, Parser};

impl<'a> Parser<'a> {
    // TODO: pass loc of string to get true start loc of declaration
    pub fn parse_variable_declaration(&mut self) -> Result<VariableDeclarationTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("variable declaration")?;

        let mutable = match self.peek_token()? {
            Token::Mut => {
                self.expect_token(Token::Mut)?;
                true
            }
            _ => false,
        };

        let declaration = VariableDeclarationTree::new(
            start_loc,
            self.parse_identifier()?,
            match self.peek_token()? {
                Token::Colon => {
                    self.expect_token(Token::Colon)?;
                    Some(self.parse_type()?)
                }
                _ => None,
            },
            mutable,
        );

        self.finish_parsing(&declaration)?;
        Ok(declaration)
    }

    pub fn parse_argument_declaration(&mut self) -> Result<ArgumentDeclarationTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("argument declaration")?;

        let mutable = match self.peek_token()? {
            Token::Mut => {
                self.expect_token(Token::Mut)?;
                true
            }
            _ => false,
        };

        let declaration = ArgumentDeclarationTree::new(
            start_loc,
            self.parse_identifier()?,
            {
                self.expect_token(Token::Colon)?;
                self.parse_type()?
            },
            mutable,
        );

        self.finish_parsing(&declaration)?;
        Ok(declaration)
    }

    pub fn parse_generic_param(&mut self) -> Result<GenericParamTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic param")?;

        let param = match self.peek_token()? {
            Token::Identifier(_) => {
                let param_name = self.parse_identifier()?;
                GenericParamTree::new(start_loc, param_name)
            }
            _ => self.unexpected_token(&[Token::Identifier("".into())])?,
        };

        self.finish_parsing(&param)?;
        Ok(param)
    }

    pub fn try_parse_generic_params_list(
        &mut self,
    ) -> Result<Option<GenericParamsListTree>, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic params list")?;
        let maybe_params_tree = match self.peek_token()? {
            Token::LThan => {
                self.expect_token(Token::LThan)?;
                let mut params_list: Vec<GenericParamTree> = Vec::new();
                loop {
                    match self.peek_token()? {
                        Token::GThan => break,
                        _ => {
                            params_list.push(self.parse_generic_param()?);
                            match self.peek_token()? {
                                Token::Comma => {
                                    self.expect_token(Token::Comma)?;
                                }
                                _ => (),
                            }
                        }
                    }
                }
                self.expect_token(Token::GThan)?;
                Some(GenericParamsListTree::new(start_loc, params_list))
            }
            _ => None,
        };

        match &maybe_params_tree {
            Some(params_tree) => self.finish_parsing(params_tree)?,
            None => self.finish_parsing(&String::from("no generic params"))?,
        }
        Ok(maybe_params_tree)
    }

    pub fn parse_identifier_with_generic_params(
        &mut self,
    ) -> Result<IdentifierWithGenericsTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("identifier with generic params")?;

        let name = self.parse_identifier()?;
        let generic_params = self.try_parse_generic_params_list()?;

        let ident_tree = IdentifierWithGenericsTree::new(start_loc, name, generic_params);
        self.finish_parsing(&ident_tree)?;
        Ok(ident_tree)
    }
}

#[cfg(test)]
mod tests {
    use super::Parser;
    use crate::{
        frontend::{
            ast::*,
            parser::{ParseError, Token},
            sourceloc::SourceLoc,
        },
        midend::types::{Mutability, Type},
        Lexer,
    };

    #[test]
    fn parse_variable_declaration() {
        let immutable_without_type = (
            "counter",
            VariableDeclarationTree::new(SourceLoc::new(1, 1), "counter".into(), None, false),
        );
        let mutable_without_type = (
            "mut counter",
            VariableDeclarationTree::new(SourceLoc::new(1, 1), "counter".into(), None, true),
        );

        let immutable_with_type = (
            "counter: u16",
            VariableDeclarationTree::new(
                SourceLoc::new(1, 1),
                "counter".into(),
                Some(TypeTree::new(SourceLoc::new(1, 10), Type::U16)),
                false,
            ),
        );
        let mutable_with_type = (
            "mut counter: u16",
            VariableDeclarationTree::new(
                SourceLoc::new(1, 1),
                "counter".into(),
                Some(TypeTree::new(SourceLoc::new(1, 14), Type::U16)),
                true,
            ),
        );

        let declaration_permutations = vec![
            immutable_without_type,
            mutable_without_type,
            immutable_with_type,
            mutable_with_type,
        ];

        for (to_parse, expected) in declaration_permutations {
            let mut p = Parser::new(Lexer::from_string(to_parse));
            assert_eq!(p.parse_variable_declaration(), Ok(expected));
        }
    }

    #[test]
    fn argument_declaration() {
        let immutable_argument = (
            "input: u32",
            ArgumentDeclarationTree::new(
                SourceLoc::new(1, 1),
                "input".into(),
                TypeTree::new(SourceLoc::new(1, 8), Type::U32),
                false,
            ),
        );
        let mutable_argument = (
            "mut input: u32",
            ArgumentDeclarationTree::new(
                SourceLoc::new(1, 1),
                "input".into(),
                TypeTree::new(SourceLoc::new(1, 12), Type::U32),
                true,
            ),
        );

        let argument_permutations = vec![immutable_argument, mutable_argument];

        for (to_parse, expected) in argument_permutations {
            let mut p = Parser::new(Lexer::from_string(to_parse));
            assert_eq!(p.parse_argument_declaration(), Ok(expected));
        }
    }

    #[test]
    fn parse_function_declaration_or_definition() {
        let mut p = Parser::new(Lexer::from_string(
            "fun declared_only()
fun declared_and_defined() {}",
        ));

        assert_eq!(
            p.parse_function_declaration_or_definition(),
            Ok(Item::FunctionDeclaration(FunctionDeclarationTree::new(
                SourceLoc::new(1, 1),
                "declared_only".into(),
                vec![],
                None,
            )))
        );

        assert_eq!(
            p.parse_function_declaration_or_definition(),
            Ok(Item::FunctionDefinition(FunctionDefinitionTree::new(
                FunctionDeclarationTree::new(
                    SourceLoc::new(2, 1),
                    "declared_and_defined".into(),
                    vec![],
                    None,
                ),
                CompoundExpressionTree::new(SourceLoc::new(2, 28), vec![],)
            )))
        );
    }

    #[test]
    fn parse_function_prototype_no_self_param() {
        let function_result = FunctionDeclarationTree::new(
            SourceLoc::new(1, 1),
            "add".into(),
            vec![
                ArgumentDeclarationTree::new(
                    SourceLoc::new(1, 9),
                    "a".into(),
                    TypeTree::new(SourceLoc::new(1, 12), Type::U32),
                    false,
                ),
                ArgumentDeclarationTree::new(
                    SourceLoc::new(1, 17),
                    "b".into(),
                    TypeTree::new(SourceLoc::new(1, 20), Type::U32),
                    false,
                ),
            ],
            Some(TypeTree::new(SourceLoc::new(1, 28), Type::U64)),
        );

        let mut p = Parser::new(Lexer::from_string("fun add(a: u32, b: u32) -> u64"));
        assert_eq!(
            p.parse_function_prototype(false),
            Ok(function_result.clone())
        );
    }

    #[test]
    fn parse_function_prototype_only_self_param() {
        let function_result = FunctionDeclarationTree::new(
            SourceLoc::new(1, 1),
            "take_self".into(),
            vec![ArgumentDeclarationTree::new(
                SourceLoc::new(1, 15),
                "self".into(),
                TypeTree::new(SourceLoc::new(1, 15), Type::_Self),
                false,
            )],
            None,
        );

        let mut p = Parser::new(Lexer::from_string("fun take_self(self)"));
        assert_eq!(p.parse_function_prototype(true), Ok(function_result));
    }

    #[test]
    fn parse_function_prototype_no_params_could_have_self() {
        let function_result = FunctionDeclarationTree::new(
            SourceLoc::new(1, 1),
            "no_params_but_could_have_self".into(),
            vec![],
            None,
        );

        let mut p = Parser::new(Lexer::from_string("fun no_params_but_could_have_self()"));
        assert_eq!(p.parse_function_prototype(true), Ok(function_result));
    }

    #[test]
    fn parse_function_prototype_fail_noncomma_nonparen() {
        let mut p = Parser::new(Lexer::from_string("fun wont_parse(a: u32 b: u32) -> u64"));

        assert_eq!(
            p.parse_function_prototype(false),
            Err(ParseError::unexpected_token(
                SourceLoc::new(1, 23),
                Token::Identifier("b".into()),
                &[Token::Comma, Token::RParen],
                "function prototype".into(),
                SourceLoc::new(1, 1),
            ))
        );
    }

    #[test]
    fn parse_function_prototype_fail_nonidentifier() {
        let mut p = Parser::new(Lexer::from_string("fun wont_parse(a: u32, u32 b) -> u64"));

        assert_eq!(
            p.parse_function_prototype(false),
            Err(ParseError::unexpected_token(
                SourceLoc::new(1, 24),
                Token::U32,
                &[Token::Identifier("".into())],
                "function prototype".into(),
                SourceLoc::new(1, 1),
            ))
        );
    }

    #[test]
    fn try_parse_self_argument() {
        let nothing = ("", None);
        let other_token = ("u8", None);
        let non_self_basic_argument = ("a: u8", None);
        let non_self_mutable_argument = ("mut a: u16", None);
        let ref_mut_illegal = ("&mut u16", None);
        let ref_illegal = ("&u16", None);
        let basic_self = (
            "self",
            Some(ArgumentDeclarationTree::new(
                SourceLoc::new(1, 1),
                "self".into(),
                TypeTree::new(SourceLoc::new(1, 1), Type::_Self),
                false,
            )),
        );
        let mut_self = (
            "mut self",
            Some(ArgumentDeclarationTree::new(
                SourceLoc::new(1, 1),
                "self".into(),
                TypeTree::new(SourceLoc::new(1, 1), Type::_Self),
                true,
            )),
        );
        let ref_self = (
            "&self",
            Some(ArgumentDeclarationTree::new(
                SourceLoc::new(1, 1),
                "self".into(),
                TypeTree::new(
                    SourceLoc::new(1, 1),
                    Type::Reference(Mutability::Immutable, Box::from(Type::_Self)),
                ),
                false,
            )),
        );
        let mut_ref_self = (
            "&mut self",
            Some(ArgumentDeclarationTree::new(
                SourceLoc::new(1, 1),
                "self".into(),
                TypeTree::new(
                    SourceLoc::new(1, 1),
                    Type::Reference(Mutability::Mutable, Box::from(Type::_Self)),
                ),
                false,
            )),
        );

        let possible_self_param_permutations = vec![
            nothing,
            other_token,
            non_self_basic_argument,
            non_self_mutable_argument,
            ref_mut_illegal,
            ref_illegal,
            basic_self,
            mut_self,
            ref_self,
            mut_ref_self,
        ];

        for (input, expected) in possible_self_param_permutations {
            let mut p = Parser::new(Lexer::from_string(input));
            assert_eq!(p.try_parse_self_argument(), Ok(expected));
        }
    }

    #[test]
    fn parse_struct_definition() {
        let mut p = Parser::new(Lexer::from_string(
            "struct Money {
dollars: i64,
cents: u8
}",
        ));

        assert_eq!(
            p.parse_struct_definition(),
            Ok(StructDefinitionTree::new(
                SourceLoc::new(1, 1),
                "Money".into(),
                vec![
                    StructFieldTree::new(
                        SourceLoc::new(2, 1),
                        "dollars".into(),
                        TypeTree::new(SourceLoc::new(2, 10), Type::I64)
                    ),
                    StructFieldTree::new(
                        SourceLoc::new(3, 1),
                        "cents".into(),
                        TypeTree::new(SourceLoc::new(3, 8), Type::U8)
                    )
                ]
            ))
        );
    }

    #[test]
    fn parse_struct_definition_fail_nonidentifier_nonrcurly() {
        let mut p = Parser::new(Lexer::from_string(
            "struct Money {
i64 dollars,
cents: u8
}",
        ));

        assert_eq!(
            p.parse_struct_definition(),
            Err(ParseError::unexpected_token(
                SourceLoc::new(2, 1),
                Token::I64,
                &[Token::Identifier("".into())],
                "struct definition".into(),
                SourceLoc::new(1, 1),
            ))
        );
    }

    #[test]
    fn parse_implementation() {
        let mut p = Parser::new(Lexer::from_string(
            "impl Money {
    fun add(self, other: &self) -> Self {
    }
}",
        ));

        let implemented_add_prototype = FunctionDeclarationTree::new(
            SourceLoc::new(2, 5),
            "add".into(),
            vec![
                ArgumentDeclarationTree::new(
                    SourceLoc::new(2, 13),
                    "self".into(),
                    TypeTree::new(SourceLoc::new(2, 13), Type::_Self),
                    false,
                ),
                ArgumentDeclarationTree::new(
                    SourceLoc::new(2, 19),
                    "other".into(),
                    TypeTree::new(
                        SourceLoc::new(2, 26),
                        Type::Reference(Mutability::Immutable, Box::from(Type::_Self)),
                    ),
                    false,
                ),
            ],
            Some(TypeTree::new(SourceLoc::new(2, 36), Type::_Self)),
        );

        let implemented_add = FunctionDefinitionTree::new(
            implemented_add_prototype,
            CompoundExpressionTree::new(SourceLoc::new(2, 41), vec![]),
        );

        let implemented_for = TypeTree::new(SourceLoc::new(1, 6), Type::Named("Money".into()));

        assert_eq!(
            p.parse_implementation(),
            Ok(ImplementationTree::new(
                SourceLoc::new(1, 1),
                implemented_for,
                vec![implemented_add]
            ))
        );
    }
}
