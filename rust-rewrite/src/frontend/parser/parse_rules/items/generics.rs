use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ItemParser<'a, 'p> {
    pub fn parse_generic_param(&mut self) -> Result<ast::generics::GenericParamTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic param")?;

        let param = match self.peek_token()? {
            Token::Identifier(_) => {
                let param_name = self.parse_identifier()?;
                ast::generics::GenericParamTree::new(start_loc, param_name)
            }
            _ => self.unexpected_token(&[Token::Identifier("".into())])?,
        };

        self.finish_parsing(param)
    }

    // parses the generic parameters to be taken by a type
    pub fn try_parse_generic_params_list(
        &mut self,
    ) -> Result<Option<ast::generics::GenericParamsListTree>, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic params list")?;
        let maybe_params_tree = match self.peek_token()? {
            Token::LThan => {
                self.expect_token(Token::LThan)?;
                let mut params_list: Vec<ast::generics::GenericParamTree> = Vec::new();
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
                Some(ast::generics::GenericParamsListTree::new(
                    start_loc,
                    params_list,
                ))
            }
            _ => None,
        };

        self.finish_parsing(maybe_params_tree)
    }

    // parses the generic parameters passed in to a type
    pub fn try_parse_generic_args_list(
        &mut self,
    ) -> Result<Option<ast::generics::GenericArgsListTree>, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic args list")?;
        let maybe_args_tree = match self.peek_token()? {
            Token::LThan => {
                self.expect_token(Token::LThan)?;
                let mut args_list: Vec<TypeTree> = Vec::new();
                loop {
                    match self.peek_token()? {
                        Token::GThan => break,
                        _ => {
                            args_list.push(self.type_parser().parse_type()?);
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
                Some(ast::generics::GenericArgsListTree::new(
                    start_loc, args_list,
                ))
            }
            _ => None,
        };

        self.finish_parsing(maybe_args_tree)
    }
}
