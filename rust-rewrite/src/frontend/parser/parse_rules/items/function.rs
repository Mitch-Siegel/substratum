use crate::frontend::{
    ast,
    parser::parse_rules::{IdentifierTree, ItemParser, ItemTree, ParseError, Token},
};

impl ItemParser<'_, '_> {
    pub(crate) fn parse_function_declaration_or_definition(
        &mut self,
    ) -> Result<ast::ItemTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("function declaration/definition")?;

        let prototype = self.parse_function_prototype(false)?;

        let decl_or_def = match self.parse_function_definition(prototype.clone()) {
            Ok(definition) => ItemTree::FunctionDefinition(definition),
            Err(_) => ItemTree::FunctionDeclaration(prototype),
        };

        self.finish_parsing(decl_or_def)
    }

    pub(crate) fn parse_function_prototype(
        &mut self,
        allow_self_param: bool,
    ) -> Result<ast::items::FunctionDeclarationTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("function prototype")?;

        // start with fun
        let fn_keyword_loc = self.expect_token(Token::Fn_)?;

        let name = self.parse_identifier()?;
        let generic_params = self.try_parse_generic_params_list()?;

        self.expect_token(Token::LParen)?;
        let mut arguments = Vec::<ast::items::function::ArgumentDeclarationTree>::new();

        if allow_self_param {
            if let Some(self_param) = self.try_parse_self_argument()? {
                arguments.push(self_param);
                if let Token::Comma = self.peek_token()? {
                    self.expect_token(Token::Comma)?;
                }
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
                _ => self.unexpected_token(&[Token::Identifier(String::new())])?,
            }
        }
        // consume closing paren
        let args_close_paren_loc = self.expect_token(Token::RParen)?;

        let return_type = match self.peek_token()? {
            Token::Arrow => {
                self.next_token()?;
                Some(self.type_parser().parse_type()?)
            }
            _ => None,
        };

        let prototype = ast::items::FunctionDeclarationTree {
            fn_keyword_loc,
            name,
            generic_params,
            arguments,
            args_close_paren_loc,
            return_type,
        };
        self.finish_parsing(prototype)
    }

    fn try_parse_self_argument(
        &mut self,
    ) -> Result<Option<ast::items::function::ArgumentDeclarationTree>, ParseError> {
        // TODO: fix loc tracking in this function

        let (start_loc, _span) = self.start_parsing("self argument")?;

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
            let self_argument = if reference {
                ast::items::function::ArgumentDeclarationTree {
                    name: IdentifierTree {
                        loc: start_loc.clone().into(),
                        value: "self".into(),
                    },

                    type_: ast::types::TypeTree::TypeNoBounds(
                        ast::types::TypeNoBoundsTree::ReferenceType(
                            ast::types::ReferenceTypeTree {
                                reference_token_loc: start_loc.clone().into(),
                                mutability: mutable.into(),
                                type_: Box::new(ast::types::TypeNoBoundsTree::TypePath(
                                    ast::types::TypePath::ItemPath(ast::types::TypeItemPathTree {
                                        underlying_path: ast::path::PathTree {
                                            starts_global: None,
                                            segments: vec![ast::path::PathSegmentTree {
                                                ident: ast::path::IdentSegment::SelfLower(
                                                    start_loc.into(),
                                                ),
                                                data: None,
                                            }],
                                        },
                                    }),
                                )),
                            },
                        ),
                    ),
                    mutable: false,
                }
            } else {
                ast::items::function::ArgumentDeclarationTree {
                    name: IdentifierTree {
                        loc: start_loc.clone().into(),
                        value: "self".into(),
                    },
                    type_: ast::types::TypeTree::TypeNoBounds(
                        ast::types::TypeNoBoundsTree::TypePath(ast::types::TypePath::ItemPath(
                            ast::types::TypeItemPathTree {
                                underlying_path: ast::path::PathTree {
                                    starts_global: None,
                                    segments: vec![ast::path::PathSegmentTree {
                                        ident: ast::path::IdentSegment::SelfLower(start_loc.into()),
                                        data: None,
                                    }],
                                },
                            },
                        )),
                    ),
                    mutable: false,
                }
            };
            Some(self_argument)
        } else {
            None
        };

        self.finish_parsing(self_argument)
    }

    pub(crate) fn parse_function_definition(
        &mut self,
        prototype: ast::items::FunctionDeclarationTree,
    ) -> Result<ast::items::FunctionDefinitionTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("function definition")?;

        let body = self.parse_block_expression()?;

        let parsed_definition = ast::items::FunctionDefinitionTree { prototype, body };
        self.finish_parsing(parsed_definition)
    }
}
