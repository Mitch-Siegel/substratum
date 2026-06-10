use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ItemParser<'a, 'p> {
    pub fn parse_generic_param(&mut self) -> Result<ast::generics::GenericParamTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("generic param")?;

        let param = match self.peek_token()? {
            Token::Identifier(_) => {
                let name = self.parse_identifier()?;
                ast::generics::GenericParamTree { name }
            }
            _ => self.unexpected_token(&[Token::Identifier("".into())])?,
        };

        self.finish_parsing(param)
    }

    // parses the generic parameters to be taken by a type
    pub fn try_parse_generic_params_list(
        &mut self,
    ) -> Result<ast::generics::OptionalGenericParamsListTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic params list")?;

        let maybe_params = match self.peek_token()? {
            Token::LThan => {
                let open_angle_bracket_loc = self.expect_token(Token::LThan)?;
                let mut params: Vec<ast::generics::GenericParamTree> = Vec::new();
                loop {
                    match self.peek_token()? {
                        Token::GThan => break,
                        _ => {
                            params.push(self.parse_generic_param()?);
                            if let Token::Comma = self.peek_token()? {
                                self.expect_token(Token::Comma)?;
                            }
                        }
                    }
                }
                let close_angle_bracket_loc = self.expect_token(Token::GThan)?;

                Some(ast::generics::GenericParamsListTree {
                    open_angle_bracket_loc,
                    params,
                    close_angle_bracket_loc,
                })
            }
            _ => None,
        };

        let optional_params_tree = ast::generics::OptionalGenericParamsListTree {
            start_loc,
            maybe_params,
        };

        self.finish_parsing(optional_params_tree)
    }

    // parses the generic parameters passed in to a type
    pub fn parse_generic_args_list(
        &mut self,
    ) -> Result<ast::generics::GenericArgsListTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("generic args list")?;

        let open_angle_bracket_loc = self.expect_token(Token::LThan)?;
        let mut args: Vec<TypeTree> = Vec::new();
        loop {
            if let Token::GThan = self.peek_token()? {
                break;
            }
            args.push(self.type_parser().parse_type()?);
            if let Token::Comma = self.peek_token()? {
                self.expect_token(Token::Comma)?;
            }
        }
        let close_angle_bracket_loc = self.expect_token(Token::GThan)?;

        let args_list = ast::generics::GenericArgsListTree {
            open_angle_bracket_loc,
            args,
            close_angle_bracket_loc,
        };

        self.finish_parsing(args_list)
    }
}
