use crate::frontend::{ast::*, lexer::token::Token};
use crate::midend;

use super::{ParseError, Parser};

impl<'a> Parser<'a> {
    // TODO: pass loc of string to get true start loc of declaration
    pub fn parse_variable_declaration(&mut self) -> Result<VariableDeclarationTree, ParseError> {
        let start_loc = self.start_parsing("variable declaration")?;

        let declaration = VariableDeclarationTree::new(start_loc, self.parse_identifier()?, {
            self.expect_token(Token::Colon)?;
            self.parse_type()?
        });

        self.finish_parsing(&declaration)?;

        Ok(declaration)
    }

    pub fn parse_function_declaration_or_definition(
        &mut self,
    ) -> Result<TranslationUnit, ParseError> {
        let _start_loc = self.start_parsing("function declaration/definition")?;

        let prototype = self.parse_function_prototype(false)?;

        let decl_or_def = match self.parse_function_definition(prototype.clone()) {
            Ok(definition) => Ok(TranslationUnit::FunctionDefinition(definition)),
            Err(_) => Ok(TranslationUnit::FunctionDeclaration(prototype)),
        };

        self.finish_parsing(decl_or_def.as_ref().unwrap())?;

        decl_or_def
    }

    fn parse_function_prototype(
        &mut self,
        allow_self_param: bool,
    ) -> Result<FunctionDeclarationTree, ParseError> {
        let start_loc = self.start_parsing("function prototype")?;

        // start with fun
        self.expect_token(Token::Fun)?;

        let name = self.parse_identifier()?;

        self.expect_token(Token::LParen)?;
        let mut arguments = Vec::<VariableDeclarationTree>::new();

        if allow_self_param {
            match self.try_parse_self_param()? {
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
                    arguments.push(self.parse_variable_declaration()?);
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

    pub fn try_parse_self_param(&mut self) -> Result<Option<VariableDeclarationTree>, ParseError> {
        let start_loc = self.start_parsing("self param")?;

        let self_param = match self.lookahead_token(0)? {
            Token::SelfLower => {
                self.expect_token(Token::SelfLower)?;
                Some(VariableDeclarationTree::new(
                    start_loc,
                    "self".into(),
                    TypeTree::new(start_loc, midend::types::Type::_Self),
                ))
            }
            Token::Reference => match self.lookahead_token(1)? {
                Token::SelfLower => {
                    println!("selflower");
                    self.expect_token(Token::Reference)?;
                    self.expect_token(Token::SelfLower)?;

                    Some(VariableDeclarationTree::new(
                        start_loc,
                        "self".into(),
                        TypeTree::new(
                            start_loc,
                            midend::types::Type::Reference(
                                midend::types::Mutability::Immutable,
                                Box::new(midend::types::Type::_Self),
                            ),
                        ),
                    ))
                }
                _ => None,
            },
            _ => None,
        };

        match &self_param {
            Some(param) => self.finish_parsing(&param)?,
            None => self.finish_parsing(&"no self param")?,
        };

        Ok(self_param)
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

    pub fn parse_struct_definition(&mut self) -> Result<StructDefinitionTree, ParseError> {
        let start_loc = self.start_parsing("struct definition")?;

        self.expect_token(Token::Struct)?;
        let struct_name = self.parse_identifier()?;
        self.expect_token(Token::LCurly)?;

        let mut struct_fields = Vec::new();

        loop {
            match self.peek_token()? {
                Token::Identifier(_) => {
                    struct_fields.push(self.parse_variable_declaration()?);

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

        let struct_definition = StructDefinitionTree::new(start_loc, struct_name, struct_fields);
        self.finish_parsing(&struct_definition)?;
        Ok(struct_definition)
    }

    pub fn parse_implementation(&mut self) -> Result<ImplementationTree, ParseError> {
        let start_loc = self.start_parsing("impl block")?;

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
        midend::types::{Mutability, Type},
        Lexer,
    };

    #[test]
    fn parse_variable_declaration() {
        let mut p = Parser::new(Lexer::from_string("counter: u16"));

        assert_eq!(
            p.parse_variable_declaration(),
            Ok(VariableDeclarationTree::new(
                SourceLoc::new(1, 1),
                "counter".into(),
                TypeTree::new(SourceLoc::new(1, 10), Type::U16,)
            ))
        );
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
                VariableDeclarationTree::new(
                    SourceLoc::new(1, 9),
                    "a".into(),
                    TypeTree::new(SourceLoc::new(1, 12), Type::U32),
                ),
                VariableDeclarationTree::new(
                    SourceLoc::new(1, 17),
                    "b".into(),
                    TypeTree::new(SourceLoc::new(1, 20), Type::U32),
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
            vec![VariableDeclarationTree::new(
                SourceLoc::new(1, 15),
                "self".into(),
                TypeTree::new(SourceLoc::new(1, 15), Type::_Self),
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
    fn try_parse_self_param() {
        let nothing = ("", Ok(None));
        let other_token = ("u8", Ok(None));
        let basic_self = (
            "self",
            Ok(Some(VariableDeclarationTree::new(
                SourceLoc::new(1, 1),
                "self".into(),
                TypeTree::new(SourceLoc::new(1, 1), Type::_Self),
            ))),
        );
        let ref_self = (
            "&self",
            Ok(Some(VariableDeclarationTree::new(
                SourceLoc::new(1, 1),
                "self".into(),
                TypeTree::new(
                    SourceLoc::new(1, 1),
                    Type::Reference(Mutability::Immutable, Box::from(Type::_Self)),
                ),
            ))),
        );
        // TODO: how should this behave? Silently not matching will cause unexpected token errors
        // in the caller. Maybe want to warn like rustc does?
        let mut_ref_self = ("&mut self", Ok(None));

        let possible_self_param_permutations =
            vec![nothing, other_token, basic_self, ref_self, mut_ref_self];

        for (input, expected) in possible_self_param_permutations {
            let mut p = Parser::new(Lexer::from_string(input));
            assert_eq!(p.try_parse_self_param(), expected);
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
                    VariableDeclarationTree::new(
                        SourceLoc::new(2, 1),
                        "dollars".into(),
                        TypeTree::new(SourceLoc::new(2, 10), Type::I64)
                    ),
                    VariableDeclarationTree::new(
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
                VariableDeclarationTree::new(
                    SourceLoc::new(2, 13),
                    "self".into(),
                    TypeTree::new(SourceLoc::new(2, 13), Type::_Self),
                ),
                VariableDeclarationTree::new(
                    SourceLoc::new(2, 19),
                    "other".into(),
                    TypeTree::new(
                        SourceLoc::new(2, 26),
                        Type::Reference(Mutability::Immutable, Box::from(Type::_Self)),
                    ),
                ),
            ],
            Some(TypeTree::new(SourceLoc::new(2, 36), Type::_Self)),
        );

        let implemented_add = FunctionDefinitionTree::new(
            implemented_add_prototype,
            CompoundExpressionTree::new(SourceLoc::new(2, 41), vec![]),
        );

        let implemented_for = TypeTree::new(SourceLoc::new(1, 6), Type::UDT("Money".into()));

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
