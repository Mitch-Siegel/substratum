use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, parse_rules::ItemParser},
};

impl ItemParser<'_, '_> {
    pub(crate) fn parse_generic_param(
        &mut self,
    ) -> Result<ast::generics::GenericParamTree, ParseError> {
        let (_start, _span) = self.start_parsing("generic param")?;

        let param = match self.peek_token()? {
            Token::Identifier(_) => {
                let name = self.parse_identifier()?;
                ast::generics::GenericParamTree { name }
            }
            _ => self.unexpected_token(&[Token::Identifier(String::new())])?,
        };

        self.finish_parsing(param)
    }

    // parses the generic parameters to be taken by a type
    pub(crate) fn try_parse_generic_params_list(
        &mut self,
    ) -> Result<ast::generics::OptionalGenericParamsListTree, ParseError> {
        let (start, _span) = self.start_parsing("generic params list")?;

        let maybe_params = match self.peek_token()? {
            Token::LThan => {
                let open_angle_bracket = self.expect_token(Token::LThan)?;
                let mut params: Vec<ast::generics::GenericParamTree> = Vec::new();
                loop {
                    if self.peek_token()? == Token::GThan {
                        break;
                    }
                    params.push(self.parse_generic_param()?);
                    if self.peek_token()? == Token::Comma {
                        self.expect_token(Token::Comma)?;
                    }
                }
                let close_angle_bracket = self.expect_token(Token::GThan)?;

                Some(ast::generics::GenericParamsListTree {
                    open_angle_bracket,
                    params,
                    close_angle_bracket,
                })
            }
            _ => None,
        };

        let optional_params_tree = ast::generics::OptionalGenericParamsListTree {
            start,
            maybe_params,
        };

        self.finish_parsing(optional_params_tree)
    }

    // parses the generic parameters passed in to a type
    pub(crate) fn parse_generic_args_list(
        &mut self,
    ) -> Result<ast::generics::GenericArgsListTree, ParseError> {
        let (_start, _span) = self.start_parsing("generic args list")?;

        let open_angle_bracket = self.expect_token(Token::LThan)?;
        let mut args: Vec<ast::TypeTree> = Vec::new();
        loop {
            if self.peek_token()? == Token::GThan {
                break;
            }
            args.push(self.type_parser().parse_type()?);
            if self.peek_token()? == Token::Comma {
                self.expect_token(Token::Comma)?;
            }
        }
        let close_angle_bracket = self.expect_token(Token::GThan)?;

        let args_list = ast::generics::GenericArgsListTree {
            open_angle_bracket,
            args,
            close_angle_bracket,
        };

        self.finish_parsing(args_list)
    }
}
