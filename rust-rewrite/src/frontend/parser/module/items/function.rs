use crate::{frontend::parser::*, midend};

impl<'a> Parser<'a> {
    pub fn parse_function_declaration_or_definition(&mut self) -> Result<Item, ParseError> {
        let (_start_loc, _span) = self.start_parsing("function declaration/definition")?;

        let prototype = self.parse_function_prototype(false)?;

        let decl_or_def = match self.parse_function_definition(prototype.clone()) {
            Ok(definition) => Item::FunctionDefinition(definition),
            Err(_) => Item::FunctionDeclaration(prototype),
        };

        self.finish_parsing(&decl_or_def)?;
        Ok(decl_or_def)
    }

    pub fn parse_function_prototype(
        &mut self,
        allow_self_param: bool,
    ) -> Result<FunctionDeclarationTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("function prototype")?;

        // start with fun
        self.expect_token(Token::Fn_)?;

        let name = self.parse_identifier_with_generic_params()?;

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

    fn try_parse_self_argument(&mut self) -> Result<Option<ArgumentDeclarationTree>, ParseError> {
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
            Some(if reference {
                ArgumentDeclarationTree::new(
                    start_loc.clone(),
                    "self".into(),
                    TypeTree::new(
                        start_loc.clone(),
                        midend::types::Type::Reference(
                            mutable.into(),
                            Box::from(midend::types::Type::_Self),
                        ),
                    ),
                    false,
                )
            } else {
                ArgumentDeclarationTree::new(
                    start_loc.clone(),
                    "self".into(),
                    TypeTree::new(start_loc, midend::types::Type::_Self),
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
}
