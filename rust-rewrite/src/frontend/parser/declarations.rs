use crate::frontend::{ast::*, lexer::token::Token};
use crate::midend;

use super::{ParseError, Parser};

impl<'a> Parser<'a> {
    // TODO: pass loc of string to get true start loc of declaration
    pub fn parse_variable_declaration(&mut self) -> Result<VariableDeclarationTree, ParseError> {
        let (start_loc, _) = self.start_parsing("variable declaration")?;

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
        let (start_loc, _) = self.start_parsing("argument declaration")?;

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

    pub fn parse_function_declaration_or_definition(
        &mut self,
    ) -> Result<TranslationUnit, ParseError> {
        let (_start_loc, _) = self.start_parsing("function declaration/definition")?;

        let prototype = self.parse_function_prototype(false)?;

        let decl_or_def = match self.parse_function_definition(prototype.clone()) {
            Ok(definition) => TranslationUnit::FunctionDefinition(definition),
            Err(_) => TranslationUnit::FunctionDeclaration(prototype),
        };

        self.finish_parsing(&decl_or_def)?;
        Ok(decl_or_def)
    }

    fn parse_function_prototype(
        &mut self,
        allow_self_param: bool,
    ) -> Result<FunctionDeclarationTree, ParseError> {
        let (start_loc, _) = self.start_parsing("function prototype")?;

        // start with fun
        self.expect_token(Token::Fun)?;

        let name = self.parse_identifier()?;

        self.expect_token(Token::LParen)?;
        let mut arguments = Vec::<ArgumentDeclarationTree>::new();

        if allow_self_param {
            match self.try_parse_self_argument()? {
                Some(self_param) => {
                    arguments.push(self_param);
                    match self.peek_token()? {
                        Token::Comma => {
                            self.expect_token(Token::Comma)?;
                        }
                        _ => {}
                    }
                }
                None => {}
            }
        }

        loop {
            match self.peek_token()? {
                // argument declaration
                Token::Identifier(_) => {
                    arguments.push(self.parse_argument_declaration()?);
                    match self.peek_token()? {
                        Token::Comma => self.next_token()?, // expect another argument declaration after comma
                        Token::RParen => break,             // loop again to handle the rparen
                        _ => self.unexpected_token(&[Token::Comma, Token::RParen])?, // everything else is uenxpected
                    };
                }
                Token::RParen => break, // done on rparen
                _ => self.unexpected_token(&[Token::Identifier("".into())])?,
            }
        }
        // consume closing paren
        self.expect_token(Token::RParen)?;

        let return_type = match self.peek_token()? {
            Token::Arrow => {
                self.next_token()?;
                Some(self.parse_type()?)
            }
            _ => None,
        };

        let prototype = FunctionDeclarationTree::new(start_loc, name, arguments, return_type);
        self.finish_parsing(&prototype)?;
        Ok(prototype)
    }

    pub fn try_parse_self_argument(
        &mut self,
    ) -> Result<Option<ArgumentDeclarationTree>, ParseError> {
        let (start_loc, _) = self.start_parsing("self argument")?;

        let (exists, mutable, reference) = match self.lookahead_token(0)? {
            // just 'self'
            Token::SelfLower => {
                self.expect_token(Token::SelfLower)?;
                (true, false, false)
            }
            // '&self' and '&mut self'
            Token::Reference => match self.lookahead_token(1)? {
                Token::SelfLower => {
                    self.expect_token(Token::Reference)?;
                    self.expect_token(Token::SelfLower)?;
                    (true, false, true)
                }
                Token::Mut => match self.lookahead_token(2)? {
                    Token::SelfLower => {
                        self.expect_token(Token::Reference)?;
                        self.expect_token(Token::Mut)?;
                        self.expect_token(Token::SelfLower)?;
                        (true, true, true)
                    }
                    _ => (false, false, false),
                },
                _ => (false, false, false),
            },
            // 'mut self'
            Token::Mut => match self.lookahead_token(1)? {
                Token::SelfLower => {
                    self.expect_token(Token::Mut)?;
                    self.expect_token(Token::SelfLower)?;

                    (true, true, false)
                }
                _ => (false, false, false),
            },
            _ => (false, false, false),
        };

        let self_argument = if exists {
            Some(if reference {
                ArgumentDeclarationTree::new(
                    start_loc,
                    "self".into(),
                    TypeTree::new(
                        start_loc,
                        midend::types::ResolvedType::Reference(
                            mutable.into(),
                            Box::from(midend::types::ResolvedType::_Self),
                        ),
                    ),
                    false,
                )
            } else {
                ArgumentDeclarationTree::new(
                    start_loc,
                    "self".into(),
                    TypeTree::new(start_loc, midend::types::ResolvedType::_Self),
                    mutable,
                )
            })
        } else {
            None
        };

        match &self_argument {
            Some(argument) => self.finish_parsing(&argument)?,
            None => self.finish_parsing(&"no self param")?,
        };

        Ok(self_argument)
    }

    fn try_parse_generic_params(&mut self) -> Result<Option<GenericParamsTree>, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic params")?;

        match self.peek_token()? {
            Token::LThan => {
                self.expect_token(Token::LThan)?;
            }
            _ => return Ok(None),
        }

        let mut params = Vec::new();

        loop {
            let param_loc = self.peek_token_with_loc()?.1;
            match self.peek_token()? {
                Token::Identifier(_) => {
                    let type_name = self.parse_identifier()?;
                    params.push(GenericParamTree {
                        loc: param_loc,
                        param: GenericParam::TypeParam(TypeParamTree {
                            loc: param_loc,
                            name: type_name,
                        }),
                    });

                    match self.peek_token()? {
                        Token::Comma => {
                            self.expect_token(Token::Comma)?;
                        }
                        _ => (),
                    }
                }
                Token::GThan => break,
                _ => self.unexpected_token(&[Token::Identifier("".into())])?,
            }
        }

        self.expect_token(Token::GThan)?;
        Ok(Some(GenericParamsTree {
            loc: start_loc,
            params,
        }))
    }

    pub fn parse_function_definition(
        &mut self,
        prototype: FunctionDeclarationTree,
    ) -> Result<FunctionDefinitionTree, ParseError> {
        self.start_parsing("function definition")?;

        let function_body = self.parse_block_expression()?;

        let parsed_definition = FunctionDefinitionTree::new(prototype, function_body);
        self.finish_parsing(&parsed_definition)?;
        Ok(parsed_definition)
    }

    pub fn parse_struct_field_declaration(&mut self) -> Result<StructFieldTree, ParseError> {
        let (start_loc, _) = self.start_parsing("struct field")?;

        let field_name = self.parse_identifier()?;
        self.expect_token(Token::Colon)?;
        let field_type = self.parse_type()?;

        let field_tree = StructFieldTree::new(start_loc, field_name, field_type);
        self.finish_parsing(&field_tree)?;
        Ok(field_tree)
    }

    pub fn parse_struct_definition(&mut self) -> Result<StructDefinitionTree, ParseError> {
        let (start_loc, _) = self.start_parsing("struct definition")?;

        self.expect_token(Token::Struct)?;
        let struct_name = self.parse_identifier()?;
        let generic_params = self.try_parse_generic_params()?;
        self.expect_token(Token::LCurly)?;

        let mut struct_fields = Vec::new();

        loop {
            match self.peek_token()? {
                Token::Identifier(_) => {
                    struct_fields.push(self.parse_struct_field_declaration()?);
                    if matches!(self.peek_token()?, Token::Comma) {
                        self.next_token()?;
                    }
                }
                Token::RCurly => {
                    self.next_token()?;
                    break;
                }
                _ => {
                    self.unexpected_token(&[Token::Identifier("".into())])?;
                }
            }
        }

        let struct_definition =
            StructDefinitionTree::new(start_loc, struct_name, struct_fields, generic_params);
        self.finish_parsing(&struct_definition)?;
        Ok(struct_definition)
    }

    pub fn parse_implementation(&mut self) -> Result<ImplementationTree, ParseError> {
        let (start_loc, _) = self.start_parsing("impl block")?;

        self.expect_token(Token::Impl)?;
        let implemented_for = self.parse_type()?;
        self.expect_token(Token::LCurly)?;

        let mut items: Vec<FunctionDefinitionTree> = Vec::new();

        while self.peek_token()? != Token::RCurly {
            let prototype = self.parse_function_prototype(true)?;
            items.push(self.parse_function_definition(prototype)?);
        }

        self.expect_token(Token::RCurly)?;

        let implementation = ImplementationTree::new(start_loc, implemented_for, items);
        self.finish_parsing(&implementation)?;
        Ok(implementation)
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
        midend::types::{Mutability, ResolvedType},
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
                Some(TypeTree::new(SourceLoc::new(1, 10), ResolvedType::U16)),
                false,
            ),
        );
        let mutable_with_type = (
            "mut counter: u16",
            VariableDeclarationTree::new(
                SourceLoc::new(1, 1),
                "counter".into(),
                Some(TypeTree::new(SourceLoc::new(1, 14), ResolvedType::U16)),
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
                TypeTree::new(SourceLoc::new(1, 8), ResolvedType::U32),
                false,
            ),
        );
        let mutable_argument = (
            "mut input: u32",
            ArgumentDeclarationTree::new(
                SourceLoc::new(1, 1),
                "input".into(),
                TypeTree::new(SourceLoc::new(1, 12), ResolvedType::U32),
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
            Ok(TranslationUnit::FunctionDeclaration(
                FunctionDeclarationTree::new(
                    SourceLoc::new(1, 1),
                    "declared_only".into(),
                    vec![],
                    None,
                )
            ))
        );

        assert_eq!(
            p.parse_function_declaration_or_definition(),
            Ok(TranslationUnit::FunctionDefinition(
                FunctionDefinitionTree::new(
                    FunctionDeclarationTree::new(
                        SourceLoc::new(2, 1),
                        "declared_and_defined".into(),
                        vec![],
                        None,
                    ),
                    CompoundExpressionTree::new(SourceLoc::new(2, 28), vec![],)
                )
            ))
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
                    TypeTree::new(SourceLoc::new(1, 12), ResolvedType::U32),
                    false,
                ),
                ArgumentDeclarationTree::new(
                    SourceLoc::new(1, 17),
                    "b".into(),
                    TypeTree::new(SourceLoc::new(1, 20), ResolvedType::U32),
                    false,
                ),
            ],
            Some(TypeTree::new(SourceLoc::new(1, 28), ResolvedType::U64)),
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
                TypeTree::new(SourceLoc::new(1, 15), ResolvedType::_Self),
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
                TypeTree::new(SourceLoc::new(1, 1), ResolvedType::_Self),
                false,
            )),
        );
        let mut_self = (
            "mut self",
            Some(ArgumentDeclarationTree::new(
                SourceLoc::new(1, 1),
                "self".into(),
                TypeTree::new(SourceLoc::new(1, 1), ResolvedType::_Self),
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
                    ResolvedType::Reference(Mutability::Immutable, Box::from(ResolvedType::_Self)),
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
                    ResolvedType::Reference(Mutability::Mutable, Box::from(ResolvedType::_Self)),
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
                        TypeTree::new(SourceLoc::new(2, 10), ResolvedType::I64)
                    ),
                    StructFieldTree::new(
                        SourceLoc::new(3, 1),
                        "cents".into(),
                        TypeTree::new(SourceLoc::new(3, 8), ResolvedType::U8)
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
                    TypeTree::new(SourceLoc::new(2, 13), ResolvedType::_Self),
                    false,
                ),
                ArgumentDeclarationTree::new(
                    SourceLoc::new(2, 19),
                    "other".into(),
                    TypeTree::new(
                        SourceLoc::new(2, 26),
                        ResolvedType::Reference(
                            Mutability::Immutable,
                            Box::from(ResolvedType::_Self),
                        ),
                    ),
                    false,
                ),
            ],
            Some(TypeTree::new(SourceLoc::new(2, 36), ResolvedType::_Self)),
        );

        let implemented_add = FunctionDefinitionTree::new(
            implemented_add_prototype,
            CompoundExpressionTree::new(SourceLoc::new(2, 41), vec![]),
        );

        let implemented_for =
            TypeTree::new(SourceLoc::new(1, 6), ResolvedType::UDT("Money".into()));

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
